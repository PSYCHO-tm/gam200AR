#include "objLoader.hpp" // exact case: Linux (and the Jenkins build) treats ObjLoader.hpp as a different, missing file

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

// Reads the OBJ line by line. The file keeps positions, uvs and normals in three separate lists and a face corner
// picks one from each ("v/vt/vn"); the GPU wants a single list of whole vertices instead. So every distinct
// (position, uv, normal) triple becomes one Vertex, and 'lookup' makes sure a triple used by several faces is
// stored only once.
std::optional<MeshData> ParseObj(std::string const& text)
{
    std::vector<Vector3D> positions; // "v"  lines, in file order
    std::vector<Vector2D> uvs;       // "vt" lines
    std::vector<Vector3D> normals;   // "vn" lines

    MeshData                                    mesh;
    std::vector<bool>                           needsNormal; // per vertex: the file gave no normal, make one below
    std::map<std::array<int, 3>, std::uint32_t> lookup;      // (position, uv, normal) indices -> vertex index

    std::istringstream lines(text);
    std::string        line;
    while (std::getline(lines, line))
    {
        std::istringstream stream(line);
        std::string        tag;
        stream >> tag; // the first word says what the line is; anything else (o, g, s, usemtl, #...) is skipped

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
                std::array<int, 3> key{-1, -1, -1}; // 0-based indices; -1 = this corner has no uv / normal
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
                if (found == lookup.end()) // first time this corner is seen: make a new Vertex for it
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
                return std::nullopt; // fewer than 3 corners is not a surface
            }
            // Fan triangulation: corners 0,1,2 then 0,2,3 then 0,3,4... Correct for the convex polygons
            // that modelling tools export (quads, mostly).
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

    // Smooth normals for vertices that had none: add up the (unnormalised) normal of every face touching the vertex,
    // then normalise. Bigger faces count more because their cross product is longer, which is what looks right.
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
std::optional<ShaderKind> ShaderKindFromName(std::string const& name)
{
    if (name == "Lit")
    {
        return ShaderKind::Lit;
    }
    return std::nullopt; // add new shaders here, next to their ShaderKind
}

std::optional<RenderMaterial> MakeMaterial(std::string const& shaderName, Vector3D const& colour)
{
    std::optional<ShaderKind> const shader = ShaderKindFromName(shaderName);
    if (!shader)
    {
        return std::nullopt;
    }
    auto const clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    RenderMaterial material;
    material.shader = *shader;
    material.colour = Vector3D{clamp01(colour.x), clamp01(colour.y), clamp01(colour.z)};
    return material;
}
