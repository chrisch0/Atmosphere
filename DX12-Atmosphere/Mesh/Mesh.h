#pragma once
#include "stdafx.h"
#include "Math\VectorMath.h"
#include "D3D12\GpuBuffer.h"
#include "BoundingBox.h"

struct SubMesh
{
	uint32_t baseVertex;
	uint32_t indexCount;
	uint32_t indexStart;
	D3D12_PRIMITIVE_TOPOLOGY topology;
	uint32_t firstVertex;
	uint32_t vertexCount;
	BoundingBox bounds;
};

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
	static Mesh* CreateSkybox();
	static Mesh* LoadModel(const std::string& name);

	Mesh() {}
	~Mesh() {}

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		return m_vertexBuffer.VertexBufferView();
	}
	
	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		return m_indexBuffer.IndexBufferView();
	}

	D3D12_PRIMITIVE_TOPOLOGY Topology() const
	{
		return m_topology;
	}

	uint32_t IndexCount() const
	{
		return m_indexCount;
	}

	void RenderSubMeshes();
	void RenderSubMesh(size_t index);

private:
	static void Subdivide(Mesh* mesh);
	static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
	static void CreateGpuBuffer(Mesh* mesh);

	std::vector<SubMesh> m_subMeshes;

	std::vector<Vertex> m_vertices;
	std::vector<uint16_t> m_indices;
	std::vector<uint32_t> m_indices32;

	StructuredBuffer m_vertexBuffer;
	ByteAddressBuffer m_indexBuffer;
	D3D12_PRIMITIVE_TOPOLOGY m_topology;

	uint32_t m_indexCount;
	uint32_t m_indexStart;
	uint32_t m_vertexBase;
};