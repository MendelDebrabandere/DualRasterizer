Texture2D gDiffuseMap	: DiffuseMap;
Texture2D gNormalMap	: NormalMap;
Texture2D gSpecularMap	: SpecularMap;
Texture2D gGlossinessMap: GlossinessMap;

float4x4 gWorldViewProj : WorldViewProjection;
float4x4 gWorldMatrix	: WorldMarix;
float4x4 gViewInverseMatrix	: ViewInverseMarix;

float gPI = 3.14159265359f;
float gLightIntensity = 7.0f;
float gShininess = 25.0f;
float3 gLightDirection = float3(0.577f, -0.577f, 0.577f);



SamplerState gSamState
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap; //or Mirror, Clamp, Border
	AddressV = Wrap; //or Mirror, Clamp, Border
};

RasterizerState gRasterizerState
{
	CullMode = back;
	FrontCounterClockwise = false; // default
};

BlendState gBlendState
{
	BlendEnable[0] = false;	
	SrcBlend = src_alpha;
	DestBlend = inv_src_alpha;
	BlendOp = add;
	SrcBlendAlpha = zero;
	DestBlendAlpha = zero;
	BlendOpAlpha = add;
	RenderTargetWriteMask[0] = 0x0F;
};

DepthStencilState gDepthStencilState
{
	DepthEnable = true;
	DepthWriteMask = 1;
	DepthFunc = less;
	StencilEnable = false;

	//others are redundant because
	//	StencilEnable is FALSE
	//(for demo purposes only)
	StencilReadMask = 0x0F;
	StencilWriteMask = 0x0F;

	FrontFaceStencilFunc = always;
	BackFaceStencilFunc = always;

	FrontFaceStencilDepthFail = keep;
	BackFaceStencilDepthFail = keep;

	FrontFaceStencilPass = keep;
	BackFaceStencilPass = keep;

	FrontFaceStencilFail = keep;
	backFaceStencilFail = keep;
};



//------------------------------------------------------
//	Input/Output Structs
//------------------------------------------------------
struct VS_INPUT
{
	float3 Position			: POSITION;
	float2 UV				: TEXCOORD;
	float3 Normal			: NORMAL;
	float3 Tangent			: TANGENT;
};

struct VS_OUTPUT
{
	float4 Position			: SV_POSITION;
	float4 WorldPosition	: WORLD_POS;
	float2 UV				: TEXCOORD;
	float3 Normal			: NORMAL;
	float3 Tangent			: TANGENT;
};


//------------------------------------------------------
//	Vertex Shader
//------------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
	VS_OUTPUT output		= (VS_OUTPUT)0;
	output.Position			= mul(float4(input.Position, 1.f), gWorldViewProj);
	output.WorldPosition	= mul(float4(input.Position, 1.f), gWorldMatrix);
	output.UV				= input.UV;
	output.Normal			= mul(normalize(input.Normal), (float3x3)gWorldMatrix);
	output.Tangent			= mul(normalize(input.Tangent), (float3x3)gWorldMatrix);

	return output;
}


//------------------------------------------------------
//	Pixel Shader
//------------------------------------------------------

float4 PS(VS_OUTPUT input) : SV_TARGET
{
	float3 binormal = cross(input.Normal, input.Tangent);
	float4x4 tangentSpaceAxis = float4x4(float4(input.Tangent, 0.0f), float4(binormal, 0.0f), float4(input.Normal, 0.0), float4(0.0f, 0.0f, 0.0f, 1.0f));

	float3 newNormal = gNormalMap.Sample(gSamState, input.UV);
	newNormal = 2.f * newNormal - float3( 1.f, 1.f, 1.f );

	newNormal = normalize(mul(newNormal, tangentSpaceAxis));

	//float3 currentNormalMap = 2.0f * gNormalMap.Sample(gSamState, input.UV).rgb - float3(1.0f, 1.0f, 1.0f);
	//float3 normal = mul(float4(currentNormalMap, 0.0f), tangentSpaceAxis);

	float3 viewDirection = normalize(input.WorldPosition.xyz - gViewInverseMatrix[3].xyz);

	// OBSERVED AREA
	float ObservedArea = saturate(dot(newNormal,  -gLightDirection));

	// DIFFUSE
	float4 TextureColor = gDiffuseMap.Sample(gSamState, input.UV) / gPI ;


	// SPECULAR
	float3 reflection = reflect(-gLightDirection, newNormal);
	float cosAlpha = saturate(dot(reflection, viewDirection));
	float specularExp = gShininess * gGlossinessMap.Sample(gSamState, input.UV).r;
	float4 specular = gSpecularMap.Sample(gSamState, input.UV) * pow(cosAlpha, specularExp);

	float4 Ambient = float4(0.025f, 0.025f, 0.025f, 0.f);

	return (gLightIntensity * TextureColor + specular) * ObservedArea + Ambient;
}

//------------------------------------------------------
//	Technique
//------------------------------------------------------
technique11 DefaultTechnique
{
	pass P0
	{
		SetRasterizerState(gRasterizerState);
		SetDepthStencilState(gDepthStencilState, 0);
		SetBlendState(gBlendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}
