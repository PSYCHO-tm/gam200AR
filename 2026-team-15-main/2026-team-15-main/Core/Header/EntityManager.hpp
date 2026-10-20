#pragma once
#include "sharedlib.hpp"

namespace Ecs{

    /*
        /brief:
            Entity contains no data / code / logic. Just an integer that holds a bitmask sig to 
            track which components belong to it.
    */
    class EntityManager{
        public:

        /*
            Default Constructor: Per construct, initialize the queue and propogate it with ordered numbers
        */
        EntityManager();

        /*
            Checks if number of living object is lesser then MAX number
            Create a unique id subsequently by taking the number of the Last index [LIFO]
        */
        Entity CreateEntity() noexcept;

         /*
            Checks if number of living object is lesser then MAX number
            Takes in a unique id given by user
        */
        Entity CreateEntityWithId(Entity id) noexcept;

        /*
            Resets all bits within sig[index] to be 0
        */
        void DestroyEntity(Entity entity) noexcept;

        /*
            Performs a bitwise copy assignment of the entire bitset passed to function
        */
        void SetSignature(Entity entity, Signature sig) noexcept;

        /*
            Returns the bitwise signature of the specific entity
        */
        Signature const& GetSignature(Entity entity) const noexcept;


        //Additional functions 
        /*
            Returns the number of living entities
        */
        uint32_t NumLivingCount() const noexcept;

        /*
            Returns whether entity is alive 
        */
        bool isLiving(Entity entity) const;


        /*
            Returns the list of entities used required by the coordinator  
        */
        std::vector<Entity> LivingEntities() const;   

        protected:
        /*
          Resets the availableEntities and signatureArrays
        */
        void Reset() noexcept;

        private:
        std::vector<Entity> availableEntities; 
        std::vector<bool> entityStatus;

        std::array<Signature, MAX_ENTITIES> signatureArr; 

        uint32_t livingCount{};   //This returns the number

        
    };
}