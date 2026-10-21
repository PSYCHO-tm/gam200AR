#include "ComponentManager.hpp"


void Ecs::ComponentManager::EntityDestroyed(Entity entity)
{
    for (auto const& pair : mComponentArrays)
    {
        auto const& component = pair.second;
        component->EntityDestroyed(entity);
    }
}

Ecs::ComponentType Ecs::ComponentManager::ComponentTypeCount() const
{
    return mNextComponentType;
}
