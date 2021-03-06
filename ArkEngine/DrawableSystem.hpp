
#pragma once

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/Glsl.hpp>
#include <SFML/System/Vector3.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "ark/ecs/System.hpp"
#include "ark/ecs/Component.hpp"
#include "ark/ecs/Entity.hpp"
#include "ark/ecs/EntityManager.hpp"
#include "ark/ecs/components/Transform.hpp"
#include "ark/ecs/Meta.hpp"

#include <vector>
#include <string>
#include <array>


    /*!
    \brief Drawable component.
    The drawable component encapsulates an sf::VertexBuffer which can be used to draw
    custom shapes, as well as being required for entities which have a sprite or text
    component. The purpose of the Drawable component is to allow mixing Sprite, Text
    and custom drawable types in a single drawing pass with variable depth. A Scene
    must have a RenderSystem added to it to enable any drawable entities.
    */
class Drawable final {
public:
    Drawable();
    Drawable(std::string);

    void setTexture(const sf::Texture* texture) { m_states.texture = texture; }
    sf::Texture* getTexture() { return const_cast<sf::Texture*>(m_states.texture); }

    void setBlendMode(sf::BlendMode mode) { m_states.blendMode = mode; }
    sf::BlendMode getBlendMode() const { return m_states.blendMode; }

    // mai mare inseamna in fata
    std::int32_t getDepth() const { return m_zDepth; }
    void setDepth(std::int32_t depth)
    {
        if (m_zDepth != depth) {
            m_zDepth = depth;
            m_wantsSorting = true;
        }
    }

    /*!
    \brief Set an area to which to crop the drawable.
    The given rectangle should be in local coordinates, relative to
    the drawable.
    */
    void setCroppingArea(sf::FloatRect area) { m_croppingArea = area; }
    sf::FloatRect getCroppingArea() const { return m_croppingArea; }

    std::vector<sf::Vertex>& getVertices() { return m_vertices; }
    const std::vector<sf::Vertex>& getVertices() const { return m_vertices; }

    /*!
    \brief Sets the PrimitiveType used by the drawable.
    Uses sf::Quads by default.
    */
    void setPrimitiveType(sf::PrimitiveType type) { m_primitiveType = type; }
    sf::PrimitiveType getPrimitiveType() const { return m_primitiveType; }

    /*!
    \brief Returns the local bounds of the Drawable's vertex array.
    This should be updated by any systems which implement custom drawables
    else culling will failed and drawable will not appear on screen
    */
    sf::FloatRect getLocalBounds() const { return m_localBounds; }

    /*!
    \brief Updates the local bounds.
    This should be called once by a system when it updates the vertex array.
    As this is used by the render system for culling, Drawable components
    will not be drawn if the bounds have not been updated.
    */
    void updateLocalBounds();
    void updateLocalBounds(sf::FloatRect rect)
    {
        m_localBounds = rect;
    }

    sf::RenderStates getStates() const { return m_states; }
    /*!
    \brief Enables or disables viewport culling.
    By default Drawables are culled from rendering when not in the
    viewable area of the active camera. Setting this to true will cause
    the drawable to always be rendered, even if it falls outside the active
    camera's view.
    \param cull Set to true to have the drawble culled from rendering when
    not intersecting the current viewable area.
    */
    void setCulled(bool cull) { m_cull = cull; }

    /*!
    \brief Enables or disables writing to the depth buffer.
    Default is true.
    */
    void setDepthWriteEnabled(bool enabled) { m_depthWriteEnabled = enabled; }
    bool getDepthWriteEnabled() const { return m_depthWriteEnabled; }

private:
    sf::PrimitiveType m_primitiveType = sf::Quads;
    sf::RenderStates m_states;
    std::vector<sf::Vertex> m_vertices;

    std::int32_t m_zDepth = 0;
    bool m_wantsSorting = true;

    sf::FloatRect m_localBounds;

    bool m_cull;

    sf::FloatRect m_croppingArea;
    sf::FloatRect m_croppingWorldArea;
    bool m_cropped;

    bool m_depthWriteEnabled;

    friend class RenderSystem;
};

ARK_REGISTER_COMPONENT(Drawable, registerServiceDefault<Drawable>()) {
    return members<Drawable>();
}

Drawable::Drawable()
    : m_primitiveType(sf::Quads),
    m_zDepth(0),
    m_wantsSorting(true),
    m_cull(true),
    m_croppingArea(std::numeric_limits<float>::lowest() / 2.f, std::numeric_limits<float>::lowest() / 2.f,
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
    m_cropped(false),
    m_depthWriteEnabled(true)
{

}

Drawable::Drawable(std::string fileName)
    : m_primitiveType(sf::Quads),
    m_zDepth(0),
    m_wantsSorting(true),
    m_cull(true),
    m_croppingArea(std::numeric_limits<float>::lowest() / 2.f, std::numeric_limits<float>::lowest() / 2.f,
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
    m_cropped(false),
    m_depthWriteEnabled(true)
{
    //m_states.texture = &texture;
    m_states.texture = ark::Resources::load<sf::Texture>(fileName);
}


void Drawable::updateLocalBounds()
{
    if (m_vertices.empty()) {
        m_localBounds = {};
        return;
    }
    //m_vertices.clear();

    auto xExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const sf::Vertex& lhs, const sf::Vertex& rhs)
    {
        return lhs.position.x < rhs.position.x;
    });

    auto yExtremes = std::minmax_element(m_vertices.begin(), m_vertices.end(),
        [](const sf::Vertex& lhs, const sf::Vertex& rhs)
    {
        return lhs.position.y < rhs.position.y;
    });

    m_localBounds.left = xExtremes.first->position.x;
    m_localBounds.top = yExtremes.first->position.y;
    m_localBounds.width = xExtremes.second->position.x - m_localBounds.left;
    m_localBounds.height = yExtremes.second->position.y - m_localBounds.top;
}

/*!
\brief Used to draw all entities which have a Drawable and Transform component.
The RenderSystem is used to depth sort and draw all entities which have a
Drawable and Transform component attached, and optionally a Sprite component.
NOTE multiple components which rely on a Drawable component cannot exist on the same entity,
as only one set of vertices will be available.
*/
// TODO de redenumit in RenderingMeshSystem, si/sau Drawable in MeshComponent
class RenderSystem final : public ark::SystemT<RenderSystem>, public ark::Renderer
{
    ark::View<ark::Transform, Drawable> view;
public:
    explicit RenderSystem();

    void init() override {
		view = entityManager.view<ark::Transform, Drawable>();
		//querry.onEntityAdd([this](ark::Entity) { this->m_wantsSorting = true; });
    }

    void update() override;

    /*!
    \brief Adds a border around the current view when culling.
    This is used to increase the effectively culled area when the system is drawn.
    By default drawables outside the view of the currently active camera are
    culled from rendering, this value adds a border around the viewble area,
    increasing or decreasing the size of the area from which drawables are culled.
    \param size The size of the border. This is a positive or negative value added
    to every side of the active view. Negative values will decrease the size of the
    culling area making drawables visibly culled from the output.
    */
    void setCullingBorder(float size);

    /*!
    \brief Returns the number of drawables rendered in the last draw call
    */
    std::size_t getDrawCount() const { return m_lastDrawCount; }

private:
    bool m_wantsSorting;
    sf::Vector2f m_cullingBorder;
    std::uint64_t m_filterFlags;

    mutable std::size_t m_lastDrawCount;
    mutable bool m_depthWriteEnabled;

    void render(sf::RenderTarget&) override;
};


RenderSystem::RenderSystem()
    : m_wantsSorting(true),
    m_lastDrawCount(0),
    m_depthWriteEnabled(true)
{
}

template <typename T>
bool utilRectContains(const sf::Rect<T>& first, const sf::Rect<T>& second)
{
    if (second.left < first.left) return false;
    if (second.top < first.top) return false;
    if (second.left + second.width > first.left + first.width) return false;
    if (second.top + second.height > first.top + first.height) return false;

    return true;
}

void RenderSystem::update()
{
    for (auto [trans, drawable] : view) {
        //auto& drawable = entity.getComponent<Drawable>();
        if (drawable.m_wantsSorting) {
            drawable.m_wantsSorting = false;
            m_wantsSorting = true;
        }

        //update cropping area
        drawable.m_cropped = !utilRectContains(drawable.m_croppingArea, drawable.m_localBounds);

        if (drawable.m_cropped) {
            //const auto& xForm = entity.getComponent<ark::Transform>().getWorldTransform();
            const auto& xForm = trans.getWorldTransform();

            //update world positions
            drawable.m_croppingWorldArea = xForm.transformRect(drawable.m_croppingArea);
            drawable.m_croppingWorldArea.top += drawable.m_croppingWorldArea.height;
            drawable.m_croppingWorldArea.height = -drawable.m_croppingWorldArea.height;
        }
    }

    //do Z sorting
    //if (m_wantsSorting) {
    //    m_wantsSorting = false;

    //    std::sort(entities.begin(), entities.end(),
    //        [](const Entity& entA, const Entity& entB)
    //    {
    //        return entA.getComponent<xy::Drawable>().getDepth() < entB.getComponent<xy::Drawable>().getDepth();
    //    });
    //}
}

void RenderSystem::setCullingBorder(float size)
{
    m_cullingBorder = { size, size };
}


void RenderSystem::render(sf::RenderTarget& rt)
{
    sf::RenderStates states;
    const auto& view = rt.getView();
    sf::FloatRect viewableArea((view.getCenter() - (view.getSize() / 2.f)) - m_cullingBorder, view.getSize() + m_cullingBorder);

    m_lastDrawCount = 0;


    //glCheck(glEnable(GL_SCISSOR_TEST));
    //glCheck(glDepthFunc(GL_LEQUAL));
    for (auto [trans, drawable] : this->view) {
        //const auto& drawable = entity.getComponent<Drawable>();
        //const auto& tx = entity.getComponent<ark::Transform>().getWorldTransform();
        const auto& tx = trans.getWorldTransform();
        const auto bounds = tx.transformRect(drawable.getLocalBounds());

        if ((!drawable.m_cull || bounds.intersects(viewableArea))) {
            //&& (drawable.m_filterFlags & m_filterFlags)) {
            states = drawable.m_states;
            states.transform = tx;

            //if (states.shader) {
            //    drawable.applyShader();
            //}

            if (drawable.m_cropped) {
                //convert cropping area to target coords (remember this might not be a window!)
                auto start = sf::Vector2f(drawable.m_croppingWorldArea.left, drawable.m_croppingWorldArea.top);
                auto end = sf::Vector2f(start.x + drawable.m_croppingWorldArea.width, start.y + drawable.m_croppingWorldArea.height);

                auto scissorStart = rt.mapCoordsToPixel(start);
                auto scissorEnd = rt.mapCoordsToPixel(end);
                //Y coords are flipped...
                auto rtHeight = rt.getSize().y;
                scissorStart.y = rtHeight - scissorStart.y;
                scissorEnd.y = rtHeight - scissorEnd.y;

                //glCheck(glScissor(scissorStart.x, scissorStart.y, scissorEnd.x - scissorStart.x, scissorEnd.y - scissorStart.y));
            }
            else {
                //just set the scissor to the view
                //auto rtSize = rt.getSize();
                //glCheck(glScissor(0, 0, rtSize.x, rtSize.y));
            }

            if (m_depthWriteEnabled != drawable.m_depthWriteEnabled) {
                //m_depthWriteEnabled = drawable.m_depthWriteEnabled;
                //glCheck(glDepthMask(m_depthWriteEnabled));
            }

            //apply any gl flags such as depth testing
            //for (auto i = 0u; i < drawable.m_glFlagIndex; ++i) {
            //    glCheck(glEnable(drawable.m_glFlags[i]));
            //}
            rt.draw(drawable.m_vertices.data(), drawable.m_vertices.size(), drawable.m_primitiveType, states);
            m_lastDrawCount++;
            //for (auto i = 0u; i < drawable.m_glFlagIndex; ++i) {
            //    glCheck(glDisable(drawable.m_glFlags[i]));
            //}
        }
    }
    //glCheck(glDisable(GL_SCISSOR_TEST));
}








