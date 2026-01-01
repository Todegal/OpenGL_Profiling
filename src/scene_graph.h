#pragma once

#include "raw_data.h"
#include "transform.h"

#include <memory>
#include <string>

class SceneNode
{
      public:
        SceneNode() = delete;
        SceneNode(const std::string& name, std::shared_ptr<SceneNode> parent = nullptr)
            : name(name), parent(parent), meshIndices(), dirtyTransform(true), children(),
              localTransform(Transform<LocalSpace, LocalSpace>::identity()),
              worldTransform(Transform<LocalSpace, WorldSpace>::identity())
        {
        }

        SceneNode(SceneNode&) = delete;
        SceneNode& operator=(SceneNode&) = delete;

        SceneNode(SceneNode&&) = delete;
        SceneNode& operator=(SceneNode&&) = delete;

        void markDirty() const
        {
                dirtyTransform = true;
                for (const auto& child : children)
                {
                        child->markDirty();
                }
        }

        void setLocalTranslation(const glm::vec3& pos)
        {
                localTranslation = pos;
                markDirty();
        }

        void setLocalRotation(const glm::quat& rot)
        {
                localRotation = rot;
                markDirty();
        }

        void setLocalRotationEuler(const glm::vec3& eulerDegrees)
        {
                localRotation = glm::quat(glm::radians(eulerDegrees));
                markDirty();
        }

        void setLocalScale(const glm::vec3& scale)
        {
                localScale = scale;
                markDirty();
        }

        void addChild(std::shared_ptr<SceneNode> child)
        {
                children.push_back(child);
        }

        void addMesh(std::size_t index)
        {
                meshIndices.push_back(index);
        }

        const std::string& getName() const
        {
                return name;
        }

        const std::vector<std::shared_ptr<SceneNode>>& getChildren() const
        {
                return children;
        }

        const std::vector<std::size_t>& getMeshIndices() const
        {
                return meshIndices;
        }

        const Transform<LocalSpace, LocalSpace>& getLocalTransform() const
        {
                if (dirtyTransform) { rebuildTransform(); }
                return localTransform;
        }

        const glm::vec3& getLocalTranslation() const
        {
                return localTranslation;
        }

        const glm::quat& getLocalRotation() const
        {
                return localRotation;
        }

        const glm::vec3 getLocalRotationEuler() const
        {
                return glm::degrees(glm::eulerAngles(localRotation));
        }

        const glm::vec3& getLocalScale() const
        {
                return localScale;
        }

        const Transform<LocalSpace, WorldSpace>& getWorldTransform() const
        {
                if (dirtyTransform) { rebuildTransform(); }
                return worldTransform;
        }

        const Point3<WorldSpace> getWorldPosition() const
        {
                return Point3<WorldSpace>(getWorldTranslation());
        }

        const glm::vec3 getWorldTranslation() const
        {
                return getWorldTransform().getTranslation();
        }

        const glm::quat getWorldRotation() const
        {
                return getWorldTransform().getRotation();
        }

        const glm::vec3 getWorldScale() const
        {
                return getWorldTransform().getScale();
        }

      private:
        std::string name;

        std::shared_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;

        glm::vec3 localTranslation{0.0f, 0.0f, 0.0f};
        glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 localScale{1.0f};

        std::vector<std::size_t> meshIndices;

        mutable bool dirtyTransform;
        mutable Transform<LocalSpace, LocalSpace> localTransform;
        mutable Transform<LocalSpace, WorldSpace> worldTransform;

        void rebuildTransform() const
        {
                localTransform =
                    Transform<LocalSpace, LocalSpace>::fromTRS(localTranslation, localRotation, localScale);

                if (parent) { worldTransform = localTransform.then(parent->getWorldTransform()); }
                else { worldTransform = Transform<LocalSpace, WorldSpace>(localTransform.getMatrix()); }

                dirtyTransform = false;
        }
};

class SceneGraph
{
      private:
        std::shared_ptr<SceneNode> rootNode;
        std::vector<std::shared_ptr<SceneNode>> nodes;
        std::unordered_map<std::string, std::size_t> indexMap;

      public:
        SceneGraph(const RawScene& rawScene);

        std::shared_ptr<SceneNode> addNode(const std::string& name, std::shared_ptr<SceneNode> parent = nullptr);

        std::shared_ptr<SceneNode> getNode(std::size_t id) const
        {
                return nodes[id];
        }

        std::shared_ptr<SceneNode> getNode(const std::string& name) const
        {
                return getNode(indexMap.at(name));
        }

        std::shared_ptr<SceneNode> getRoot() const
        {
                return rootNode;
        }

        const std::vector<std::shared_ptr<SceneNode>>& getNodes() const
        {
                return nodes;
        }
};
