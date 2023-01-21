#pragma once
#include "DataTypes.h"

namespace dae
{
	struct VertexD11 final //HARDWARE
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;
		Vector3 tangent;
	};

	class SoftwareShader;
	class Effect;
	class Texture;

	class Mesh final
	{
	public:
		Mesh(ID3D11Device* pDevice, const std::string& objectPath, Effect* pEffect,
			Texture* pDiffuse, Texture* pNormal, Texture* pSpecular, Texture* pGlossiness);
		~Mesh();

		// rule of 5 copypasta
		Mesh(const Mesh& other) = delete;
		Mesh(Mesh&& other) = delete;
		Mesh& operator=(const Mesh& other) = delete;
		Mesh& operator=(Mesh&& other) = delete;

		void Update(const Timer* pTimer);
		void SetMatrix(const Matrix& matrix, Matrix* invViewMatrix);
		void ToggleRotation();
		void SetSamplerState(ID3D11SamplerState* pSampleState);
		void SetCullMode(ID3D11RasterizerState* newCullMode);

		void RenderDirectX(ID3D11DeviceContext* pDeviceContext) const;

		void VertexTransformationFunction();
		const std::vector<uint32_t>& GetIndices() const { return indices; }
		const std::vector<Vertex_Out>& GetVerticesOut() const { return vertices_out; }

		const Texture* GetDiffuse() const { return m_pDiffuse; }
		const Texture* GetNormal() const { return m_pNormalMap; }
		const Texture* GetSpecular() const { return m_pSpecularMap; }
		const Texture* GetGlossiness() const { return m_pGlossinessMap; }
	private:
		void InitMesh(ID3D11Device* pDevice, const std::vector<VertexD11>& vertices, const std::vector<uint32_t>& indices);


		Effect* m_pEffect{ nullptr };
		ID3D11InputLayout* m_pInputLayout{ nullptr };
		ID3D11Buffer* m_pVertexBuffer{ nullptr };

		uint32_t m_NumIndices{};
		ID3D11Buffer* m_pIndexBuffer{ nullptr };

		const Matrix m_StartWorldMatrix{ Matrix::CreateTranslation(0,0,50) };
		Matrix m_WorldMatrix{ m_StartWorldMatrix };
		Matrix m_ViewInverse{};
		Matrix m_WorldViewProjectionMatrix{};

		bool m_IsRotating{ false };
		float m_Rotation{};

		//SOFTWARE
		std::vector<Vertex> vertices{};
		std::vector<uint32_t> indices{};
		std::vector<Vertex_Out> vertices_out{};
		Texture* m_pDiffuse{ nullptr };
		Texture* m_pNormalMap{ nullptr };
		Texture* m_pSpecularMap{ nullptr };
		Texture* m_pGlossinessMap{ nullptr };
	};
}


