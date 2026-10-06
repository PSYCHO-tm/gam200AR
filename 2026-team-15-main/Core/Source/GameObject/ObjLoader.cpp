#include "../../Header/GameObject/MeshData.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Core
{
	namespace
	{
		// Resolves a 1-based (or negative, relative) OBJ index into a 0-based one, or -1 if out of range.
		int ResolveIndex(long raw, std::size_t count)
		{
			long const index = raw > 0 ? raw - 1 : static_cast<long>(count) + raw;
			return (index >= 0 && index < static_cast<long>(count)) ? static_cast<int>(index) : -1;
		}

		struct FaceCorner
		{
			int Position = -1;
			int Normal   = -1;
		};

		bool ParseCorner(std::string const& token, std::size_t positionCount, std::size_t normalCount, FaceCorner& out)
		{
			std::size_t const firstSlash = token.find('/');
			long const        p          = std::strtol(token.c_str(), nullptr, 10);
			out.Position                 = ResolveIndex(p, positionCount);
			if (out.Position < 0)
			{
				return false;
			}
			if (firstSlash == std::string::npos)
			{
				return true;
			}
			std::size_t const secondSlash = token.find('/', firstSlash + 1);
			if (secondSlash != std::string::npos && secondSlash + 1 < token.size())
			{
				long const n = std::strtol(token.c_str() + secondSlash + 1, nullptr, 10);
				out.Normal   = ResolveIndex(n, normalCount);
				if (out.Normal < 0)
				{
					return false;
				}
			}
			return true;
		}
	}

	bool ParseObj(std::string const& text, MeshData& outMesh, std::string& outError)
	{
		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
		MeshData          mesh;

		std::istringstream stream(text);
		std::string        line;
		int                lineNumber = 0;
		while (std::getline(stream, line))
		{
			++lineNumber;
			std::istringstream words(line);
			std::string        tag;
			words >> tag;
			if (tag == "v" || tag == "vn")
			{
				Vec3 value;
				if (!(words >> value.x >> value.y >> value.z))
				{
					outError = "OBJ line " + std::to_string(lineNumber) + ": expected 3 numbers after '" + tag + "'";
					return false;
				}
				(tag == "v" ? positions : normals).push_back(value);
			}
			else if (tag == "f")
			{
				std::vector<FaceCorner> corners;
				std::string             token;
				while (words >> token)
				{
					FaceCorner corner;
					if (!ParseCorner(token, positions.size(), normals.size(), corner))
					{
						outError = "OBJ line " + std::to_string(lineNumber) + ": index out of range in '" + token + "'";
						return false;
					}
					corners.push_back(corner);
				}
				if (corners.size() < 3)
				{
					outError = "OBJ line " + std::to_string(lineNumber) + ": a face needs at least 3 vertices";
					return false;
				}
				for (std::size_t i = 1; i + 1 < corners.size(); ++i)
				{
					FaceCorner const tri[3]     = {corners[0], corners[i], corners[i + 1]};
					Vec3 const       a          = positions[tri[0].Position];
					Vec3 const       b          = positions[tri[1].Position];
					Vec3 const       c          = positions[tri[2].Position];
					Vec3 const       faceNormal = Normalize(Cross(b - a, c - a));
					for (FaceCorner const& corner : tri)
					{
						Vec3 const p = positions[corner.Position];
						Vec3 const n = corner.Normal >= 0 ? normals[corner.Normal] : faceNormal;
						mesh.Vertices.insert(mesh.Vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
					}
				}
			}
		}

		if (mesh.Vertices.empty())
		{
			outError = "OBJ contains no faces";
			return false;
		}
		mesh.Bounds.Min = mesh.Bounds.Max = {mesh.Vertices[0], mesh.Vertices[1], mesh.Vertices[2]};
		for (std::size_t i = 0; i < mesh.Vertices.size(); i += 6)
		{
			float const* v  = &mesh.Vertices[i];
			mesh.Bounds.Min = {std::min(mesh.Bounds.Min.x, v[0]), std::min(mesh.Bounds.Min.y, v[1]),
			                   std::min(mesh.Bounds.Min.z, v[2])};
			mesh.Bounds.Max = {std::max(mesh.Bounds.Max.x, v[0]), std::max(mesh.Bounds.Max.y, v[1]),
			                   std::max(mesh.Bounds.Max.z, v[2])};
		}
		outMesh = mesh;
		return true;
	}
}
