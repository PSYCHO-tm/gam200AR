#include "objLoader.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <sstream>
#include <vector>

namespace
{
    // Resolves a 1-based (or negative, relative) OBJ index to 0-based. False when out of range.
    bool ResolveIndex(int raw, std::size_t count, int& out)
    {
        if (raw > 0)
        {
            out = raw - 1;
        }
        else if (raw < 0)
        {
            out = static_cast<int>(count) + raw;
        }
        else
        {
            return false;
        }
        return out >= 0 && static_cast<std::size_t>(out) < count;
    }

    // "p", "p/t", "p//n" or "p/t/n" -> raw ints (0 = absent). False on non-numeric text.
    bool SplitFaceToken(std::string const& token, std::array<int, 3>& raw)
    {
        raw              = {0, 0, 0};
        std::size_t part = 0;
        std::string current;
        for (std::size_t i = 0; i <= token.size() && part < 3; ++i)
        {
            if (i == token.size() || token[i] == '/')
            {
                if (!current.empty())
                {
                    try
                    {
                        raw[part] = std::stoi(current);
                    }
                    catch (...)
                    {
                        return false;
                    }
                }
                current.clear();
                ++part;
            }
            else
            {
                current.push_back(token[i]);
            }
        }
        return true;
    }
}

std::optional<MeshData> ParseObj(std::string const& text)
{
    std::vector<Vector3D> positions;
    std::vector<Vector2D> uvs;
    std::vector<Vector3D> normals;

    MeshData                                    mesh;
    std::vector<bool>                           needsNormal;
    std::map<std::array<int, 3>, std::uint32_t> lookup;

    std::istringstream lines(text);
    std::string        line;
    while (std::getline(lines, line))
    {
        std::istringstream stream(line);
        std::string        tag;
        stream >> tag;

        if (tag == "v")
        {
            Vector3D p;
            stream >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (tag == "vt")
        {
            Vector2D t;
            stream >> t.x >> t.y;
            uvs.push_back(t);
        }
        else if (tag == "vn")
        {
            Vector3D n;
            stream >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (tag == "f")
        {
            std::vector<std::uint32_t> face;
            std::string                token;
            while (stream >> token)
            {
                std::array<int, 3> raw{};
                if (!SplitFaceToken(token, raw))
                {
                    return std::nullopt;
                }
                std::array<int, 3> key{-1, -1, -1};
                if (!ResolveIndex(raw[0], positions.size(), key[0]))
                {
                    return std::nullopt;
                }
                if (raw[1] != 0 && !ResolveIndex(raw[1], uvs.size(), key[1]))
                {
                    return std::nullopt;
                }
                if (raw[2] != 0 && !ResolveIndex(raw[2], normals.size(), key[2]))
                {
                    return std::nullopt;
                }

                auto found = lookup.find(key);
                if (found == lookup.end())
                {
                    Vertex v;
                    v.pos = positions[static_cast<std::size_t>(key[0])];
                    if (key[1] >= 0)
                    {
                        v.uv = uvs[static_cast<std::size_t>(key[1])];
                    }
                    if (key[2] >= 0)
                    {
                        v.normal = normals[static_cast<std::size_t>(key[2])];
                    }
                    found = lookup.emplace(key, static_cast<std::uint32_t>(mesh.vertices.size())).first;
                    mesh.vertices.push_back(v);
                    needsNormal.push_back(key[2] < 0);
                }
                face.push_back(found->second);
            }
            if (face.size() < 3)
            {
                return std::nullopt;
            }
            for (std::size_t i = 1; i + 1 < face.size(); ++i)
            {
                mesh.indices.push_back(face[0]);
                mesh.indices.push_back(face[i]);
                mesh.indices.push_back(face[i + 1]);
            }
        }
    }

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        return std::nullopt;
    }

    // Smooth normals for vertices that had none.
    std::vector<Vector3D> sums(mesh.vertices.size());
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        std::uint32_t const a = mesh.indices[i];
        std::uint32_t const b = mesh.indices[i + 1];
        std::uint32_t const c = mesh.indices[i + 2];
        Vector3D const      faceNormal =
            (mesh.vertices[b].pos - mesh.vertices[a].pos).CrossProduct(mesh.vertices[c].pos - mesh.vertices[a].pos);
        for (std::uint32_t index : {a, b, c})
        {
            if (needsNormal[index])
            {
                sums[index] = sums[index] + faceNormal;
            }
        }
    }
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        if (needsNormal[i])
        {
            Vector3D const n        = sums[i].Normalize();
            mesh.vertices[i].normal = (n.Magnitude() > 0.0f) ? n : Vector3D{0.0f, 1.0f, 0.0f};
        }
    }
    return mesh;
}