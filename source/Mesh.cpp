#include "pch.h"
#include "Mesh.h"
#include "Effect.h"
#include <cassert>
#include "utils.h"
#include "Texture.h"

using namespace dae;

Mesh::Mesh(ID3D11Device* pDevice, const std::string& objectPath, Effect* pEffect,
			Texture* pDiffuse, Texture* pNormal, Texture* pSpecular, Texture* pGlossiness)
	: m_pEffect{ pEffect }
	, m_pDiffuse{ pDiffuse }
	, m_pNormalMap{ pNormal }
	, m_pSpecularMap{ pSpecular }
	, m_pGlossinessMap{ pGlossiness }
{
	Utils::ParseOBJ(objectPath, vertices, indices);

	std::vector<VertexD11> verticesD11{};
	for (const Vertex& vtx : vertices)
	{
		verticesD11.push_back(VertexD11{ vtx.position, vtx.uv, vtx.normal, vtx.tangent });
	}

	m_pInputLayout = m_pEffect->LoadInputLayout(pDevice);
	m_pEffect->SetDiffuseMap(pDiffuse);
	m_pEffect->SetNormalMap(pNormal);
	m_pEffect->SetSpecularMap(pSpecular);
	m_pEffect->SetGlossinessMap(pGlossiness);

	InitMesh(pDevice, verticesD11, indices);
}

void Mesh::VertexTransformationFunction()
{
	vertices_out.clear();
	vertices_out.reserve(vertices.size());

	for (const Vertex& vtx : vertices)
	{

		Vertex_Out vertexOut{};

		// to NDC-Space
		vertexOut.position = m_WorldViewProjectionMatrix.TransformPoint({ vtx.position, 1.0f });

		// The viewdirection is just the coordinates of the vertex after being transformed to viewspace
		vertexOut.viewDirection = vertexOut.position.GetXYZ();
		vertexOut.viewDirection.Normalize();

		vertexOut.position.x /= vertexOut.position.w;
		vertexOut.position.y /= vertexOut.position.w;
		vertexOut.position.z /= vertexOut.position.w;

		vertexOut.uv = vtx.uv;
		vertexOut.normal = m_WorldMatrix.TransformVector(vtx.normal);
		//vertexOut.normal.Normalize();
		vertexOut.tangent = m_WorldMatrix.TransformVector(vtx.tangent);
		//vertexOut.tangent.Normalize();

		vertices_out.emplace_back(vertexOut);

	}
}

void Mesh::InitMesh(ID3D11Device* pDevice, const std::vector<VertexD11>& vertices, const std::vector<uint32_t>& indices)
{
	//Create Vertex buffer
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = sizeof(VertexD11) * static_cast<uint32_t>(vertices.size());
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices.data();

	HRESULT result = pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
	if (FAILED(result))
		return;


	//Create Index Buffer
	m_NumIndices = static_cast<uint32_t>(indices.size());
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.ByteWidth = sizeof(uint32_t) * m_NumIndices;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	initData.pSysMem = indices.data();
	result = pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
	if (FAILED(result))
		return;
}

Mesh::~Mesh()
{
	if (m_pDiffuse) delete m_pDiffuse;
	if (m_pNormalMap) delete m_pNormalMap;
	if (m_pSpecularMap) delete m_pSpecularMap;
	if (m_pGlossinessMap) delete m_pGlossinessMap;

	delete m_pEffect;

	if (m_pVertexBuffer) m_pVertexBuffer->Release();

	if (m_pIndexBuffer) m_pIndexBuffer->Release();
}

void Mesh::Update(const Timer* pTimer)
{
	if (m_IsRotating)
	{
		constexpr float rotationSpeed{ 45 };
		m_Rotation += pTimer->GetElapsed() * rotationSpeed/ 180.f * PI;
		m_WorldMatrix = Matrix::CreateRotationY(m_Rotation) * m_StartWorldMatrix;
	}
}

void Mesh::RenderDirectX(ID3D11DeviceContext* pDeviceContext) const
{
	//1. Set Primitive Topology
	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//2. Set Input Layout
	pDeviceContext->IASetInputLayout(m_pInputLayout);

	//3. Set VertexBuffer
	constexpr UINT stride = sizeof(VertexD11);
	constexpr UINT offset = 0;
	pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	//4. Set IndexBuffer
	pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	//5. Draw
	D3DX11_TECHNIQUE_DESC techDesc{};
	m_pEffect->GetTechnique()->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		m_pEffect->GetTechnique()->GetPassByIndex(p)->Apply(0, pDeviceContext);
		pDeviceContext->DrawIndexed(m_NumIndices, 0, 0);
	}


}

void Mesh::SetMatrix(const Matrix& matrix, Matrix* invViewMatrix)
{
	m_ViewInverse = *invViewMatrix;
	m_WorldViewProjectionMatrix = m_WorldMatrix * matrix;
	m_pEffect->SetWorldViewProjMatrix(reinterpret_cast<float*>(&m_WorldViewProjectionMatrix));
	m_pEffect->SetWorldMatrix(reinterpret_cast<float*>(&m_WorldMatrix));
	m_pEffect->SetInverseViewMatrix(reinterpret_cast<float*>(invViewMatrix));
}

void Mesh::ToggleRotation()
{
	m_IsRotating = !m_IsRotating;
}

void Mesh::SetSamplerState(ID3D11SamplerState* pSampleState)
{
	m_pEffect->SetSampleState(pSampleState);
}

void Mesh::SetCullMode(ID3D11RasterizerState* newCullMode)
{
	m_pEffect->SetCullMode(newCullMode);
}


 