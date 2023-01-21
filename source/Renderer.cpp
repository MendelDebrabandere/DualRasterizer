#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "Camera.h"
#include "EffectShaded.h"
#include "EffectTransparent.h"
#include "Utils.h"
#include "Texture.h"

namespace dae {

	Renderer::Renderer(SDL_Window* pWindow) :
		m_pWindow(pWindow)
	{
		//Initialize
		SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

		//Initialize DirectX pipeline
		const HRESULT result = InitializeDirectX();
		if (result == S_OK)
		{
			m_IsInitialized = true;
			std::cout << "DirectX is initialized and ready!\n";
		}
		else
		{
			std::cout << "DirectX initialization failed!\n";
		}

		//Create Buffers
		m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
		m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
		m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

		m_pDepthBufferPixels = new float[m_Width * m_Height];

		InitMeshes();

		m_pCamera = new Camera();
		m_pCamera->Initialize(float(m_Width) / m_Height, 45.f, { 0,0,0 });

	}

	Renderer::~Renderer()
	{
		delete[] m_pDepthBufferPixels;
		delete m_pCamera;
		for (Mesh* pMesh : m_MeshPtrs)
		{
			delete pMesh;
		}

		if (m_pRenderTargetView) m_pRenderTargetView->Release();
		if (m_pRenderTargetBuffer) m_pRenderTargetBuffer->Release();

		if (m_pDepthStencilView) m_pDepthStencilView->Release();
		if (m_pDepthStencilBuffer) m_pDepthStencilBuffer->Release();

		if (m_pSwapChain) m_pSwapChain->Release();

		if (m_pDeviceContext)
		{
			m_pDeviceContext->ClearState();
			m_pDeviceContext->Flush();
			m_pDeviceContext->Release();
		}
		if (m_pDevice) m_pDevice->Release();
	}

	void Renderer::Update(const Timer* pTimer)
	{
		m_pCamera->Update(pTimer);
		for (Mesh* pMesh : m_MeshPtrs)
		{
			pMesh->SetMatrix(m_pCamera->GetViewMatrix() * m_pCamera->GetProjectionMatrix(), m_pCamera->GetInvViewMatrix());

			pMesh->Update(pTimer);
		}
	}


	void Renderer::Render() const
	{
		if (!m_IsInitialized)
			return;


		if (m_UseDirectX) //HARDWARE DIRECTX
		{
			RenderDirectX();
		}
		else //SOFTWARE
		{
			RenderSoftware();
		}
	}

	void Renderer::RenderDirectX() const
	{
		//1. CLEAR RTV & DSV
		ColorRGB clearColor{ 0.39f, 0.59f, 0.93f };
		if (m_UsingUniformClearColor)
			clearColor = { 0.1f,0.1f,0.1f };
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, &clearColor.r);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

		//2. SET PIPELINE + INVOKE DRAWCALLS (= RENDER)
		for (const Mesh* pMesh : m_MeshPtrs)
		{
			pMesh->RenderDirectX(m_pDeviceContext);
			if (m_RenderFire == false)
				break;
		}

		//3. PRESENT BACKBUFFER (SWAP)
		m_pSwapChain->Present(0, 0);
	}

	void Renderer::RenderSoftware() const
	{
		//@START
		//Lock BackBuffer
		SDL_LockSurface(m_pBackBuffer);

		// Fill the array with max float value
		std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);

		ClearBackground();


		// Only render vehicle
		Mesh* mesh = m_MeshPtrs[0];
		mesh->VertexTransformationFunction();

		const std::vector<uint32_t> indices{ mesh->GetIndices() };
		const std::vector<Vertex_Out> vertices_out{ mesh->GetVerticesOut() };

		// For every triangle
		for (int currIdx{}; currIdx < indices.size(); ++currIdx)
		{
			int vertexIdx0{};
			int vertexIdx1{};
			int vertexIdx2{};

			// Updating vertexindeces using trianglelist primitiveTopology
			vertexIdx0 = indices[currIdx];
			vertexIdx1 = indices[currIdx + 1];
			vertexIdx2 = indices[currIdx + 2];
			currIdx += 2;

			if (vertexIdx0 == vertexIdx1 || vertexIdx1 == vertexIdx2 || vertexIdx0 == vertexIdx2)
				continue;

			if (!IsInFrustum(vertices_out[vertexIdx0])
				|| !IsInFrustum(vertices_out[vertexIdx1])
				|| !IsInFrustum(vertices_out[vertexIdx2]))
				continue;

			Vertex_Out Vertex0{ NDCToScreen(vertices_out[vertexIdx0]) };
			Vertex_Out Vertex1{ NDCToScreen(vertices_out[vertexIdx1]) };
			Vertex_Out Vertex2{ NDCToScreen(vertices_out[vertexIdx2]) };

			//Setting up some variables
			const Vector2 v0{ Vertex0.position.GetXY() };
			const Vector2 v1{ Vertex1.position.GetXY() };
			const Vector2 v2{ Vertex2.position.GetXY() };

			const float depthV0{ Vertex0.position.z };
			const float depthV1{ Vertex1.position.z };
			const float depthV2{ Vertex2.position.z };

			const Vector2 edge01{ v1 - v0 };
			const Vector2 edge12{ v2 - v1 };
			const Vector2 edge20{ v0 - v2 };

			const float areaTriangle{ fabs(Vector2::Cross(v1 - v0, v2 - v0)) };

			if (areaTriangle <= 0.01f)
			{
				continue;
			}

			// create bounding box for triangle
			const int bottom = std::min(int(std::min(v0.y, v1.y)), int(v2.y));
			const int top = std::max(int(std::max(v0.y, v1.y)), int(v2.y)) + 1;

			const int left = std::min(int(std::min(v0.x, v1.x)), int(v2.x));
			const int right = std::max(int(std::max(v0.x, v1.x)), int(v2.x)) + 1;

			// check if bounding box is in screen
			if (left <= 0 || right >= m_Width - 1)
				continue;

			if (bottom <= 0 || top >= m_Height - 1)
				continue;

			constexpr int offSet{ 1 };

			////RENDER LOGIC
			//for (int px{}; px < m_Width; ++px)
			//{
			//	for (int py{}; py < m_Height; ++py)
			//	{


			for (int px = left - offSet; px < right + offSet; ++px)
			{
				for (int py = bottom - offSet; py < top + offSet; ++py)
				{
					ColorRGB finalColor = colors::Black;

					if (m_Visualize == Visualize::BoundingBox)
					{
						finalColor = colors::White;
					}
					else
					{
						Vector2 pixel = { float(px), float(py) };

						// Maths to check if the pixel is in the triangle
						const Vector2 directionV0 = pixel - v0;
						const Vector2 directionV1 = pixel - v1;
						const Vector2 directionV2 = pixel - v2;

						float weightV2 = Vector2::Cross(edge01, directionV0);
						float weightV0 = Vector2::Cross(edge12, directionV1);
						float weightV1 = Vector2::Cross(edge20, directionV2);

						// Calculate which side of the triangle has been hit
						const bool isFrontFaceHit{ weightV2 > 0 && weightV0 > 0 && weightV1 > 0 };
						const bool isBackFaceHit{ weightV2 < 0 && weightV0 < 0 && weightV1 < 0 };

						// Continue to the next pixel if the cullmode demands it
						if ((m_CullMode == CullMode::Back && !isFrontFaceHit) ||
							(m_CullMode == CullMode::Front && !isBackFaceHit) ||
							(m_CullMode == CullMode::None && !isFrontFaceHit /*&& !isBackFaceHit*/))
							continue;

						// Setting up the weights for the UV coordinates
						weightV0 /= areaTriangle;
						weightV1 /= areaTriangle;
						weightV2 /= areaTriangle;

						// Create BufferValue
						const float ZBufferVal{
							1.f /
							((1 / depthV0) * weightV0 +
							(1 / depthV1) * weightV1 +
							(1 / depthV2) * weightV2)
						};

						// Check if there is no triangle in front of this triangle
						if (ZBufferVal > m_pDepthBufferPixels[px * m_Height + py])
							continue;

						// Add BufferValue to the array
						m_pDepthBufferPixels[px * m_Height + py] = ZBufferVal;


						// Visualize what is requested by user
						switch (m_Visualize)
						{
						case Visualize::FinalColor:
						{
							// Interpolating all atributes
							// for shading we use world coordinates

							const Vertex_Out v0_world{ vertices_out[vertexIdx0] };
							const Vertex_Out v1_world{ vertices_out[vertexIdx1] };
							const Vertex_Out v2_world{ vertices_out[vertexIdx2] };

							const float interpolatedWDepth = {
								1.f /
								((1 / v0_world.position.w) * weightV0 +
								(1 / v1_world.position.w) * weightV1 +
								(1 / v2_world.position.w) * weightV2)
							};

							const Vector2 interpolatedUV = {
								((v0_world.uv / v0_world.position.w) * weightV0 +
								(v1_world.uv / v1_world.position.w) * weightV1 +
								(v2_world.uv / v2_world.position.w) * weightV2) * interpolatedWDepth
							};

							Vector3 interpolatedNormal = {
								((v0_world.normal / v0_world.position.w) * weightV0 +
								(v1_world.normal / v1_world.position.w) * weightV1 +
								(v2_world.normal / v2_world.position.w) * weightV2) * interpolatedWDepth
							};
							interpolatedNormal.Normalize();

							Vector3 interpolatedTangent = {
								((v0_world.tangent / v0_world.position.w) * weightV0 +
								(v1_world.tangent / v1_world.position.w) * weightV1 +
								(v2_world.tangent / v2_world.position.w) * weightV2) * interpolatedWDepth
							};
							interpolatedTangent.Normalize();

							Vector3 interpolatedViewDirection = {
								((v0_world.viewDirection / v0_world.position.w) * weightV0 +
								(v1_world.viewDirection / v1_world.position.w) * weightV1 +
								(v2_world.viewDirection / v2_world.position.w) * weightV2) * interpolatedWDepth
							};
							interpolatedViewDirection.Normalize();

							//Interpolated Vertex Attributes for Pixel
							Vertex_Out pixelVertex;
							pixelVertex.position = Vector4{ pixel.x, pixel.y, ZBufferVal, interpolatedWDepth };
							pixelVertex.uv = interpolatedUV;
							pixelVertex.normal = interpolatedNormal;
							pixelVertex.tangent = interpolatedTangent;
							pixelVertex.viewDirection = interpolatedViewDirection;

							finalColor = PixelShading(pixelVertex, *mesh);

							break;
						}
						case Visualize::DepthBuffer:
						{
							constexpr float depthRemapSize{ 0.005f };

							float remapedBufferVal{ ZBufferVal };
							DepthRemap(remapedBufferVal, depthRemapSize);
							finalColor = ColorRGB{ remapedBufferVal, remapedBufferVal , remapedBufferVal };
							break;
						}
						}
					}

					//Update Color in Buffer
					finalColor.MaxToOne();

					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}
			}
		}


		//@END
		//Update SDL Surface
		SDL_UnlockSurface(m_pBackBuffer);
		SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
		SDL_UpdateWindowSurface(m_pWindow);

	}

	void Renderer::ToggleRotation()
	{
		for (Mesh* pMesh : m_MeshPtrs)
		{
			pMesh->ToggleRotation();
		}
	}

	void Renderer::CycleCullModes()
	{
		switch (m_CullMode)
		{
		case CullMode::None:
			m_CullMode = CullMode::Back;
			std::cout << "Using back culling\n";
			break;
		case CullMode::Back:
			m_CullMode = CullMode::Front;
			std::cout << "Using front culling\n";
			break;
		case CullMode::Front:
			m_CullMode = CullMode::None;
			std::cout << "Using no culling\n";
			break;
		}

		// Set the new rasterizerState
		D3D11_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.FrontCounterClockwise = false;
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.SlopeScaledDepthBias = 0.0f;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.DepthClipEnable = true;
		rasterizerDesc.ScissorEnable = false;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false;

		switch (m_CullMode)
		{
		case CullMode::None:
		{
			rasterizerDesc.CullMode = D3D11_CULL_NONE;
			break;
		}
		case CullMode::Back:
		{
			rasterizerDesc.CullMode = D3D11_CULL_BACK;
			break;
		}
		case CullMode::Front:
		{
			rasterizerDesc.CullMode = D3D11_CULL_FRONT;
			break;
		}
		}

		// Release the current rasterizer state if one exists
		if (m_pRasterizerVariable) m_pRasterizerVariable->Release();

		// Create a new rasterizer state
		HRESULT hr{ m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerVariable) };
		if (FAILED(hr)) std::wcout << L"m_pRasterizerState failed to load\n";

		m_MeshPtrs[0]->SetCullMode(m_pRasterizerVariable);
	}

	void Renderer::ToggleFilteringMethod()
	{
		D3D11_FILTER newFilter{};
		switch (m_FilteringMethod)
		{
		case FilteringMethod::Point:
			m_FilteringMethod = FilteringMethod::Linear;
			newFilter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			std::cout << "FILTERING METHOD: LINEAR\n";
			break;
		case FilteringMethod::Linear:
			m_FilteringMethod = FilteringMethod::Anisotropic;
			newFilter = D3D11_FILTER_ANISOTROPIC;
			std::cout << "FILTERING METHOD: ANISOTROPIC\n";
			break;
		case FilteringMethod::Anisotropic:
			m_FilteringMethod = FilteringMethod::Point;
			newFilter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			std::cout << "FILTERING METHOD: POINT\n";
			break;
		}

		LoadSampleState(newFilter, m_pDevice);
	}

	void Renderer::ToggleDirectX()
	{
		m_UseDirectX = !m_UseDirectX;
		if (m_UseDirectX)
			std::cout << "Using hardware rendering\n";
		else
			std::cout << "Using software rendering\n";
	}

	void Renderer::ToggleUniformClearColor()
	{
		m_UsingUniformClearColor = !m_UsingUniformClearColor;
	}

	void Renderer::ToggleFireRendering()
	{
		m_RenderFire = !m_RenderFire;
	}

	void Renderer::CycleShadingMode()
	{
		switch (m_ShadingMode)
		{
		case ShadingMode::ObservedArea:
			m_ShadingMode = ShadingMode::Diffuse;
			std::cout << "Using Diffuse ShadingMode in software\n";
			break;
		case ShadingMode::Diffuse:
			m_ShadingMode = ShadingMode::Specular;
			std::cout << "Using Specular ShadingMode in software\n";
			break;
		case ShadingMode::Specular:
			m_ShadingMode = ShadingMode::Combined;
			std::cout << "Using Combined ShadingMode in software\n";
			break;
		case ShadingMode::Combined:
			m_ShadingMode = ShadingMode::ObservedArea;
			std::cout << "Using ObservedArea ShadingMode in software\n";
			break;
		}
	}

	void Renderer::ToggleNormalMap()
	{
		m_UseNormalMap = !m_UseNormalMap;
		if (m_UseNormalMap)
			std::cout << "Normal map is enabled in software\n";
		else
			std::cout << "Normal map is disabled in software\n";
	}

	void Renderer::ToggleDepthBufferVisualization()
	{
		if (m_Visualize != Visualize::DepthBuffer)
			m_Visualize = Visualize::DepthBuffer;
		else
			m_Visualize = Visualize::FinalColor;
	}

	void Renderer::ToggleBoundingBoxVisualization()
	{
		if (m_Visualize != Visualize::BoundingBox)
			m_Visualize = Visualize::BoundingBox;
		else
			m_Visualize = Visualize::FinalColor;
	}

	void Renderer::InitMeshes()
	{
		//Vehicle
		EffectShaded* vehicleEffect{ new EffectShaded{ m_pDevice, L"Resources/PosCol3D.fx" } };

		//Load textures
		Texture* pDiffuse{ Texture::LoadFromFile("Resources/vehicle_diffuse.png", m_pDevice) };
		Texture* pNormal{ Texture::LoadFromFile("Resources/vehicle_normal.png", m_pDevice) };
		Texture* pSpecular{ Texture::LoadFromFile("Resources/vehicle_specular.png", m_pDevice) };
		Texture* pGlossiness{ Texture::LoadFromFile("Resources/vehicle_gloss.png", m_pDevice) };

		//Create vehicle
		Mesh* pVehicle{ new Mesh{ m_pDevice, "Resources/vehicle.obj", vehicleEffect
								, pDiffuse, pNormal, pSpecular, pGlossiness} };
		m_MeshPtrs.push_back(pVehicle);



		//Fire
		EffectTransparent* fireEffect{ new EffectTransparent{ m_pDevice, L"Resources/PartialCoverage.fx" } };

		//Load textures
		Texture* pFireDiffuse{ Texture::LoadFromFile("Resources/fireFX_diffuse.png", m_pDevice) };

		//Create fire
		Mesh* pFire{ new Mesh{ m_pDevice, "Resources/fireFX.obj", fireEffect, 
							pFireDiffuse, nullptr, nullptr, nullptr	} };
		m_MeshPtrs.push_back(pFire);


	}

	HRESULT Renderer::InitializeDirectX()
	{
		//1. Create Device & DeviceContext
		//=====
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
		uint32_t createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, &featureLevel,
											1, D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pDeviceContext);

		if (FAILED(result))
			return result;

		//Create DXGI Factory
		IDXGIFactory1* pDxgiFactory{};
		result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pDxgiFactory));
		if (FAILED(result))
			return result;



		//2. Create Swapchain
		//=====
		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		swapChainDesc.BufferDesc.Width = m_Width;
		swapChainDesc.BufferDesc.Height = m_Height;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 1;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 60;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.Windowed = true;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = 0;

		// Get the handle (HWND) from the SDL Backbuffer
		SDL_SysWMinfo sysWMInfo{};
		SDL_VERSION(&sysWMInfo.version)
		SDL_GetWindowWMInfo(m_pWindow, &sysWMInfo);
		swapChainDesc.OutputWindow = sysWMInfo.info.win.window;

		//Create SwapChain
		result = pDxgiFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
		if (FAILED(result))
			return result;

		//3. Create DepthStencil (DS) & DepthStencilView (DSV)
		//Resource
		D3D11_TEXTURE2D_DESC depthStencilDesc{};
		depthStencilDesc.Width = m_Width;
		depthStencilDesc.Height = m_Height;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		//View
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
		depthStencilViewDesc.Format = depthStencilDesc.Format;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		result = m_pDevice->CreateTexture2D(&depthStencilDesc, nullptr, &m_pDepthStencilBuffer);
		if (FAILED(result))
			return result;

		result = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView);
		if (FAILED(result))
			return result;


		//4. Create RenderTarget (RT) & RenderTargetView (RTV)
		//=====

		//Resource
		result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pRenderTargetBuffer));
		if (FAILED(result))
			return result;

		//View
		result = m_pDevice->CreateRenderTargetView(m_pRenderTargetBuffer, nullptr, &m_pRenderTargetView);
		if (FAILED(result))
			return result;


		//5. Bind RTV & DSV to Output Merger Stage
		//=====
		m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);


		//6. Set Viewport
		//=====
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(m_Width);
		viewport.Height = static_cast<float>(m_Height);
		viewport.TopLeftX = 0.f;
		viewport.TopLeftY = 0.f;
		viewport.MinDepth = 0.f;
		viewport.MaxDepth = 1.f;
		m_pDeviceContext->RSSetViewports(1, &viewport);

		return result;

	}

	void Renderer::LoadSampleState(const D3D11_FILTER& filter, ID3D11Device* device)
	{
		// Create the SampleState description
		D3D11_SAMPLER_DESC sampleDesc{};
		sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampleDesc.MipLODBias = 0;
		sampleDesc.MinLOD = 0;
		sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;
		sampleDesc.MaxAnisotropy = 16;
		sampleDesc.Filter = filter;


		if (m_pSamplerState) m_pSamplerState->Release();


		HRESULT result{ device->CreateSamplerState(&sampleDesc, &m_pSamplerState) };
		if (FAILED(result))
			std::cout << "m_pSamplerState failed to load\n";

		for (Mesh* pMesh : m_MeshPtrs)
		{
			pMesh->SetSamplerState(m_pSamplerState);
		}

	}

	void dae::Renderer::ClearBackground() const
	{
		
		if (m_UsingUniformClearColor)
			SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 0.1f * 265, 0.1f * 265, 0.1f * 265));
		else
			SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 0.39f * 265, 0.39f * 265, 0.39f * 265));
	}

	Vertex_Out Renderer::NDCToScreen(const Vertex_Out& vtx) const
	{
		Vertex_Out vertex{ vtx };
		vertex.position.x = (vtx.position.x + 1.f) * 0.5f * float(m_Width);
		vertex.position.y = (1.f - vtx.position.y) * 0.5f * float(m_Height);
		return vertex;
	}

	bool Renderer::IsInFrustum(const Vertex_Out& vtx)
	{
		if (vtx.position.x < -1 || vtx.position.x > 1)
			return false;

		if (vtx.position.y < -1 || vtx.position.y > 1)
			return  false;

		if (vtx.position.z < 0 || vtx.position.z > 1)
			return  false;

		return true;
	}

	void Renderer::DepthRemap(float& depth, float topPercentile) const
	{
		depth = (depth - (1.f - topPercentile)) / topPercentile;

		depth = std::max(0.f, depth);
		depth = std::min(1.f, depth);
	}

	ColorRGB Renderer::PixelShading(Vertex_Out v, const Mesh& mesh) const
	{
		// Light settings
		const Vector3 lightDirection{ 0.577f, -0.577f, 0.577f };
 		constexpr float lightIntensity{ 7.f };
		constexpr float specularShininess{ 25.f };


		if (m_UseNormalMap)
		{
			const Vector3 biNormal = Vector3::Cross(v.normal, v.tangent);
			const Matrix tangentSpaceAxis = { v.tangent, biNormal, v.normal, Vector3::Zero };

			const ColorRGB normalColor = mesh.GetNormal()->Sample(v.uv);
			Vector3 sampledNormal = { normalColor.r, normalColor.g, normalColor.b };
			sampledNormal = 2.f * sampledNormal - Vector3{ 1.f, 1.f, 1.f };

			sampledNormal = tangentSpaceAxis.TransformVector(sampledNormal);

			v.normal = sampledNormal.Normalized();
		}


		// OBSERVED AREA
		float ObservedArea{ Vector3::Dot(v.normal,  -lightDirection) };
		ObservedArea = std::max(ObservedArea, 0.f);

		// DIFFUSE
		const ColorRGB TextureColor{ mesh.GetDiffuse()->Sample(v.uv) / PI};

		// SPECULAR
		const Vector3 reflect{ Vector3::Reflect(-lightDirection, v.normal) };
		float cosAlpha{ Vector3::Dot(reflect, v.viewDirection) };
		cosAlpha = std::max(0.f, cosAlpha);
		const float specularExp{ specularShininess * mesh.GetGlossiness()->Sample(v.uv).r };

		const ColorRGB specular{ mesh.GetSpecular()->Sample(v.uv) * powf(cosAlpha, specularExp)};

		ColorRGB finalColor{ 0,0,0 };

		switch (m_ShadingMode)
		{
		case ShadingMode::ObservedArea:
		{
			finalColor = ColorRGB{ ObservedArea ,ObservedArea ,ObservedArea };
			break;
		}
		case ShadingMode::Diffuse:
		{
			finalColor = lightIntensity * TextureColor;
			break;
		}
		case ShadingMode::Specular:
		{

			finalColor = specular;// *observedAreaRGB;
			break;
		}
		case ShadingMode::Combined:
		{
			finalColor = (lightIntensity * TextureColor + specular) * ObservedArea;
			break;
		}
		}

		constexpr ColorRGB ambient{ 0.025f, 0.025f, 0.025f };

		finalColor += ambient;

		finalColor.MaxToOne();

		return finalColor;
	}

}
