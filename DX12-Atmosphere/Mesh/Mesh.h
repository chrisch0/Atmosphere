#pragma once
#include "stdafx.h"
#include "Math\VectorMath.h"
#include "D3D12\GpuBuffer.h"

class Mesh
{
	friend class MeshRenderer;
public:
	struct Vertex
	{
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			TangentU(t),
			TexC(uv) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TangentU(tx, ty, tz),
			TexC(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT2 TexC;
	};

	static Mesh* CreateSphere(uint16_t sliceCount, uint16_t stackCount);
	static Mesh* CreateGeosphere(uint32_t numSubdivisions);
	static Mesh* CreateBox(uint32_t numSubdivisions);
	static Mesh* CreateQuad(float x, float y, float w, float h, float depth);
	//static Mesh* CreateCylinder();
	static Mesh* CreateGrid(uint16_t m, uint16_t n);

	Mesh() {}
	~Mesh() {}

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()
	{
		return m_vertexBuffer.VertexBufferView();
	}
	
	D3D12_INDEX_BUFFER_VIEW IndexBufferView()
	{
		return m_indexBuffer.IndexBufferView();
	}

private:
	static void Subdivide(Mesh* mesh);
	static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
	static void CreateGpuBuffer(Mesh* mesh);

	std::vector<Vertex> m_vertices;
	std::vector<uint16_t> m_indices;

	StructuredBuffer m_vertexBuffer;
	ByteAddressBuffer m_indexBuffer;
};