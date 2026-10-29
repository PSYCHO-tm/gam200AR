#include "picking.hpp"

std::optional<PickHit> PickNearest(Ray const& ray, std::vector<Collider> const& worldBounds)
{
    std::optional<PickHit> best;
    for (std::size_t i = 0; i < worldBounds.size(); ++i)
    {
        float distance = 0.0f;
        if (worldBounds[i].IntersectsRay(ray, distance) && (!best || distance < best->distance))
        {
            best = PickHit{i, distance};
        }
    }
    return best;
}

std::optional<PickHit> PickEquipment(Ray const& ray, EquipmentLibrary const& library,
                                     std::vector<Matrix4x4> const& models)
{
    std::vector<Collider> worldBounds;
    worldBounds.reserve(library.items.size());
    for (std::size_t i = 0; i < library.items.size(); ++i)
    {
        worldBounds.push_back(i < models.size() ? library.items[i].localBounds.Transformed(models[i]) : Collider{});
    }
    return PickNearest(ray, worldBounds);
}
