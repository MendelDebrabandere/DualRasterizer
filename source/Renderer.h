#pragma once
#include "DataTypes.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{

	class Mesh;
	class Camera;

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(const Timer* pTimer);
		void Render() const;

		//COMBINED
		void ToggleDirectX();
		void ToggleRotation();
		void CycleCullModes();
		void ToggleUniformClearColor();
		//HARDWARE
		void ToggleFilteringMethod();
		void ToggleFireRendering();
		//SOFTWARE
		void CycleShadingMode();
		void ToggleNormalMap();
		void ToggleDepthBufferVisualization();
		void ToggleBoundingBoxVisualization();
	private:
		void RenderDirectX() const;
		void RenderSoftware() const;

		void InitMeshes();

		bool m_UseDirectX{ true };
		bool m_UsingUniformClearColor{ false };
		bool m_RenderFire{ true };

		SDL_Window* m_pWindow{};

		int m_Width{};
		int m_Height{};

		bool m_IsInitialized{ false };

		std::vector<Mesh*> m_MeshPtrs{};
		Camera* m_pCamera{ nullptr };

		ID3D11SamplerState* m_pSamplerState{ nullptr };
		ID3D11Device* m_pDevice{ nullptr };
		ID3D11DeviceContext* m_pDeviceContext{ nullptr };
		IDXGISwapChain* m_pSwapChain{ nullptr };
		ID3D11Texture2D* m_pDepthStencilBuffer{ nullptr };
		ID3D11DepthStencilView* m_pDepthStencilView{ nullptr };
		ID3D11RenderTargetView* m_pRenderTargetView{ nullptr };
		ID3D11Resource* m_pRenderTargetBuffer{ nullptr };
		ID3D11RasterizerState* m_pRasterizerVariable{ nullptr };


		//DIRECTX
		HRESULT InitializeDirectX();
		//...

		enum class FilteringMethod
		{
			Point = 0,
			Linear = 1,
			Anisotropic = 2
		};

		FilteringMethod m_FilteringMethod{ 0 };

		void LoadSampleState(const D3D11_FILTER& filter, ID3D11Device* device);

		//SOFTWARE
		void ClearBackground() const;
		Vertex_Out NDCToScreen(const Vertex_Out& vtx) const;
		static bool IsInFrustum(const Vertex_Out& vtx);
		void DepthRemap(float& depth, float topPercentile) const;
		ColorRGB PixelShading(Vertex_Out v, const Mesh& mesh) const;

		enum class CullMode
		{
			None = 0,
			Back = 1,
			Front = 2,
		};
		CullMode m_CullMode{ CullMode::Back };
		enum class Visualize {
			FinalColor = 0,
			DepthBuffer = 1,
			BoundingBox = 2
		};
		Visualize m_Visualize{ Visualize::FinalColor };
		enum class ShadingMode {
			ObservedArea = 0,
			Diffuse = 1,
			Specular = 2,
			Combined = 3
		};
		ShadingMode m_ShadingMode{ ShadingMode::Combined };


		bool m_UseNormalMap{true};
		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};
		float* m_pDepthBufferPixels{};


	};
}
