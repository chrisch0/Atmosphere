#include "stdafx.h"
#include "Mesh.h"
#include "SkyboxMesh.h"

void Mesh::CreateGpuBuffer(Mesh* mesh)
{
	mesh->m_vertexBuffer.Create(L"Vertices", (uint32_t)mesh->m_vertices.size(), (uint32_t)sizeof(Vertex), mesh->m_vertices.data());
	mesh->m_indexBuffer.Create(L"Indices", (uint32_t)mesh->m_indices.size(), (uint32_t)sizeof(uint16_t), mesh->m_indices.data());
	mesh->m_indexCount = (uint32_t)mesh->m_indices.size();
	mesh->m_vertices.clear();
	mesh->m_indices.clear();
}

Mesh* Mesh::CreateBox(uint32_t numSubdivisions)
{
	Mesh* mesh = new Mesh();
	Vertex v[24];

	// Fill in the front face vertex data.
	//            Position          Normal             Tangent           UV            
	v[0] = Vertex(-0.5, -0.5, -0.5, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(-0.5, +0.5, -0.5, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex(+0.5, +0.5, -0.5, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex(+0.5, -0.5, -0.5, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex(-0.5, -0.5, +0.5, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[5] = Vertex(+0.5, -0.5, +0.5, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex(+0.5, +0.5, +0.5, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[7] = Vertex(-0.5, +0.5, +0.5, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	v[8] = Vertex(-0.5, +0.5, -0.5, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9] = Vertex(-0.5, +0.5, +0.5, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex(+0.5, +0.5, +0.5, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex(+0.5, +0.5, -0.5, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex(-0.5, -0.5, -0.5, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[13] = Vertex(+0.5, -0.5, -0.5, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[14] = Vertex(+0.5, -0.5, +0.5, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[15] = Vertex(-0.5, -0.5, +0.5, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex(-0.5, -0.5, +0.5, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	v[17] = Vertex(-0.5, +0.5, +0.5, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex(-0.5, +0.5, -0.5, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	v[19] = Vertex(-0.5, -0.5, -0.5, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex(+0.5, -0.5, -0.5, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex(+0.5, +0.5, -0.5, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+0.5, +0.5, +0.5, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex(+0.5, -0.5, +0.5, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	mesh->m_vertices.assign(&v[0], &v[24]);

	//
	// Create the indices.
	//

	uint16_t i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	mesh->m_indices.assign(&i[0], &i[36]);

	// Put a cap on the number of subdivisions.
	numSubdivisions = std::min<uint32_t>(numSubdivisions, 6u);

	for (uint32_t i = 0; i < numSubdivisions; ++i)
		Subdivide(mesh);

	CreateGpuBuffer(mesh);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

Mesh* Mesh::CreateSphere(uint16_t sliceCount, uint16_t stackCount)
{
	Mesh* mesh = new Mesh();
	Vertex topVertex(0.0f, 0.5f, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex bottomVertex(0.0f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	mesh->m_vertices.push_back(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f*XM_PI / sliceCount;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for (uint16_t i = 1; i <= stackCount - 1; ++i)
	{
		float phi = i * phiStep;

		// Vertices of ring.
		for (uint16_t j = 0; j <= sliceCount; ++j)
		{
			float theta = j * thetaStep;

			Vertex v;

			// spherical to cartesian
			v.Position.x = 0.5f * sinf(phi)*cosf(theta);
			v.Position.y = 0.5f * cosf(phi);
			v.Position.z = 0.5f * sinf(phi)*sinf(theta);

			// Partial derivative of P with respect to theta
			v.TangentU.x = -0.5f * sinf(phi)*sinf(theta);
			v.TangentU.y = 0.0f;
			v.TangentU.z = +0.5f * sinf(phi)*cosf(theta);

			XMVECTOR T = XMLoadFloat3(&v.TangentU);
			XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TexC.x = theta / XM_2PI;
			v.TexC.y = phi / XM_PI;

			mesh->m_vertices.push_back(v);
		}
	}

	mesh->m_vertices.push_back(bottomVertex);

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

	for (uint16_t i = 1; i <= sliceCount; ++i)
	{
		mesh->m_indices.push_back(0);
		mesh->m_indices.push_back(i + 1);
		mesh->m_indices.push_back(i);
	}

	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	uint16_t baseIndex = 1;
	uint16_t ringVertexCount = sliceCount + 1;
	for (uint16_t i = 0; i < stackCount - 2; ++i)
	{
		for (uint16_t j = 0; j < sliceCount; ++j)
		{
			mesh->m_indices.push_back(baseIndex + i * ringVertexCount + j);
			mesh->m_indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			mesh->m_indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);

			mesh->m_indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);
			mesh->m_indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			mesh->m_indices.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint16_t southPoleIndex = (uint16_t)mesh->m_vertices.size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (uint16_t i = 0; i < sliceCount; ++i)
	{
		mesh->m_indices.push_back(southPoleIndex);
		mesh->m_indices.push_back(baseIndex + i);
		mesh->m_indices.push_back(baseIndex + i + 1);
	}

	CreateGpuBuffer(mesh);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

Mesh* Mesh::CreateGeosphere(uint32_t numSubdivisions)
{
	Mesh* mesh = new Mesh();

	// Put a cap on the number of subdivisions.
	numSubdivisions = std::min<uint32_t>(numSubdivisions, 6u);

	// Approximate a sphere by tessellating an icosahedron.

	const float X = 0.525731f;
	const float Z = 0.850651f;

	XMFLOAT3 pos[12] =
	{
		XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),
		XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),
		XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X),
		XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),
		XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f),
		XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
	};

	uint16_t k[60] =
	{
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0,
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7
	};

	mesh->m_vertices.resize(12);
	mesh->m_indices.assign(&k[0], &k[60]);

	for (uint32_t i = 0; i < 12; ++i)
		mesh->m_vertices[i].Position = pos[i];

	for (uint16_t i = 0; i < numSubdivisions; ++i)
		Subdivide(mesh);

	// Project vertices onto sphere and scale.
	for (uint32_t i = 0; i < mesh->m_vertices.size(); ++i)
	{
		// Project onto unit sphere.
		XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mesh->m_vertices[i].Position));

		// Project onto sphere.
		XMVECTOR p = 0.5f * n;

		XMStoreFloat3(&mesh->m_vertices[i].Position, p);
		XMStoreFloat3(&mesh->m_vertices[i].Normal, n);

		// Derive texture coordinates from spherical coordinates.
		float theta = atan2f(mesh->m_vertices[i].Position.z, mesh->m_vertices[i].Position.x);

		// Put in [0, 2pi].
		if (theta < 0.0f)
			theta += XM_2PI;

		float phi = acosf(mesh->m_vertices[i].Position.y / 0.5f);

		mesh->m_vertices[i].TexC.x = theta / XM_2PI;
		mesh->m_vertices[i].TexC.y = phi / XM_PI;

		// Partial derivative of P with respect to theta
		mesh->m_vertices[i].TangentU.x = -0.5f * sinf(phi)*sinf(theta);
		mesh->m_vertices[i].TangentU.y = 0.0f;
		mesh->m_vertices[i].TangentU.z = +0.5f * sinf(phi)*cosf(theta);

		XMVECTOR T = XMLoadFloat3(&mesh->m_vertices[i].TangentU);
		XMStoreFloat3(&mesh->m_vertices[i].TangentU, XMVector3Normalize(T));
	}

	CreateGpuBuffer(mesh);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

Mesh* Mesh::CreateGrid(uint16_t m, uint16_t n)
{
	Mesh* mesh = new Mesh();

	uint16_t vertexCount = m * n;
	uint16_t faceCount = (m - 1)*(n - 1) * 2;

	//
	// Create the vertices.
	//

	float halfWidth = 0.5f*10.0f;
	float halfDepth = 0.5f*10.0f;

	float dx = 10.0f / (n - 1);
	float dz = 10.0f / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	mesh->m_vertices.resize(vertexCount);
	for (uint16_t i = 0; i < m; ++i)
	{
		float z = halfDepth - i * dz;
		for (uint16_t j = 0; j < n; ++j)
		{
			float x = -halfWidth + j * dx;

			mesh->m_vertices[i*n + j].Position = XMFLOAT3(x, 0.0f, z);
			mesh->m_vertices[i*n + j].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			mesh->m_vertices[i*n + j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			mesh->m_vertices[i*n + j].TexC.x = j * du;
			mesh->m_vertices[i*n + j].TexC.y = i * dv;
		}
	}

	//
	// Create the indices.
	//

	mesh->m_indices.resize(faceCount * 3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint16_t k = 0;
	for (uint16_t i = 0; i < m - 1; ++i)
	{
		for (uint16_t j = 0; j < n - 1; ++j)
		{
			mesh->m_indices[k] = i * n + j;
			mesh->m_indices[k + 1] = i * n + j + 1;
			mesh->m_indices[k + 2] = (i + 1)*n + j;

			mesh->m_indices[k + 3] = (i + 1)*n + j;
			mesh->m_indices[k + 4] = i * n + j + 1;
			mesh->m_indices[k + 5] = (i + 1)*n + j + 1;

			k += 6; // next quad
		}
	}

	CreateGpuBuffer(mesh);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

// (x, y) for left bottom screen coord
// (x, y) = (0, 0), (w, h) = (1, 1) SV_POSITION = (x, y, depth, 1) for full screen quad
// uv (0, 0) is the left top
Mesh* Mesh::CreateQuad(float x, float y, float w, float h, float depth)
{
	Mesh* mesh = new Mesh();

	mesh->m_vertices.resize(4);
	mesh->m_indices.resize(6);

	// Position coordinates specified in NDC space.
	mesh->m_vertices[0] = Vertex(
		2.0f * x - 1.0f, 2.0f * y - 1.0f, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f);

	mesh->m_vertices[1] = Vertex(
		2.0f * x - 1.0f, 2.0f * (y + h) - 1.0f, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f);

	mesh->m_vertices[2] = Vertex(
		2.0f * (x + w) - 1.0f, 2.0f * (y + h) - 1.0f, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	mesh->m_vertices[3] = Vertex(
		2.0f * (x + w) - 1.0f, 2.0f * y - 1.0f, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	mesh->m_indices[0] = 0;
	mesh->m_indices[1] = 1;
	mesh->m_indices[2] = 2;

	mesh->m_indices[3] = 0;
	mesh->m_indices[4] = 2;
	mesh->m_indices[5] = 3;

	CreateGpuBuffer(mesh);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	return mesh;
}

Mesh* Mesh::CreateSkybox()
{
	Mesh* mesh = new Mesh();
	mesh->m_vertexBuffer.Create(L"SkyboxVertices", 5040, (uint32_t)(sizeof(float) * 3), SkyboxVertices);
	mesh->m_indexBuffer.Create(L"SkyboxIndices", 5040, (uint32_t)sizeof(uint16_t), SkyboxIndices);
	mesh->m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mesh->m_indexCount = 5040;
	return mesh;
}

void Mesh::Subdivide(Mesh* mesh)
{
	// Save a copy of the input geometry.
	std::vector<Vertex> vertices = mesh->m_vertices;
	std::vector<uint16_t> indices = mesh->m_indices;

	mesh->m_vertices.resize(0);
	mesh->m_indices.resize(0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	uint16_t numTris = (uint16_t)indices.size() / 3;
	for (uint16_t i = 0; i < numTris; ++i)
	{
		Vertex v0 = vertices[indices[i * 3 + 0]];
		Vertex v1 = vertices[indices[i * 3 + 1]];
		Vertex v2 = vertices[indices[i * 3 + 2]];

		//
		// Generate the midpoints.
		//

		Vertex m0 = MidPoint(v0, v1);
		Vertex m1 = MidPoint(v1, v2);
		Vertex m2 = MidPoint(v0, v2);

		//
		// Add new geometry.
		//

		mesh->m_vertices.push_back(v0); // 0
		mesh->m_vertices.push_back(v1); // 1
		mesh->m_vertices.push_back(v2); // 2
		mesh->m_vertices.push_back(m0); // 3
		mesh->m_vertices.push_back(m1); // 4
		mesh->m_vertices.push_back(m2); // 5

		mesh->m_indices.push_back(i * 6 + 0);
		mesh->m_indices.push_back(i * 6 + 3);
		mesh->m_indices.push_back(i * 6 + 5);

		mesh->m_indices.push_back(i * 6 + 3);
		mesh->m_indices.push_back(i * 6 + 4);
		mesh->m_indices.push_back(i * 6 + 5);

		mesh->m_indices.push_back(i * 6 + 5);
		mesh->m_indices.push_back(i * 6 + 4);
		mesh->m_indices.push_back(i * 6 + 2);

		mesh->m_indices.push_back(i * 6 + 3);
		mesh->m_indices.push_back(i * 6 + 1);
		mesh->m_indices.push_back(i * 6 + 4);
	}
}

Mesh::Vertex Mesh::MidPoint(const Vertex& v0, const Vertex& v1)
{
	XMVECTOR p0 = XMLoadFloat3(&v0.Position);
	XMVECTOR p1 = XMLoadFloat3(&v1.Position);

	XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
	XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

	XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
	XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

	XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
	XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

	// Compute the midpoints of all the attributes.  Vectors need to be normalized
	// since linear interpolating can make them not unit length.  
	XMVECTOR pos = 0.5f*(p0 + p1);
	XMVECTOR normal = XMVector3Normalize(0.5f*(n0 + n1));
	XMVECTOR tangent = XMVector3Normalize(0.5f*(tan0 + tan1));
	XMVECTOR tex = 0.5f*(tex0 + tex1);

	Vertex v;
	XMStoreFloat3(&v.Position, pos);
	XMStoreFloat3(&v.Normal, normal);
	XMStoreFloat3(&v.TangentU, tangent);
	XMStoreFloat2(&v.TexC, tex);

	return v;
}