Texture3D<float> perlin : register(t0);
Texture3D<float> worleyFBM_Low : register(t1);
Texture3D<float> worleyFBM_Mid : register(t2);
Texture3D<float> worleyFBM_High : register(t3);

RWTexture3D<float4> basic_shape_texture : register(u0);
RWTexture2D<float4> basic_shape_view : register(u1);

SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);

float Remap(float original_value,
			float original_min, float original_max,
			float new_min, float new_max)
{
	return new_min + ((original_value - original_min) / (original_max - original_min) * (new_max - new_min));
}

[numthreads(8, 8, 8)]
void main( uint3 globalID : SV_DispatchThreadID )
{
	float p = perlin[globalID].x;
	float4 basic_shape;
	basic_shape.y = worleyFBM_Low[globalID].x;
	basic_shape.x = Remap(p, basic_shape.y, 1.0, 0.0, 1.0);
	if (globalID.z == 0)
		basic_shape_view[globalID.xy] = float4(basic_shape.xxx, 1.0);
	basic_shape.z = worleyFBM_Mid[globalID].x;
	basic_shape.w = worleyFBM_High[globalID].x;
	basic_shape_texture[globalID] = basic_shape;
}