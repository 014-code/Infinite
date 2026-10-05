#include "TestSupport.h"
#include "support/ColorDepthTarget.h"
#include "graphics/rendering/shadows/DirectionalShadowMap.h"
#include "graphics/rendering/HdrPipeline.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/camera/Camera.h"
#include "graphics/geometry/Mesh.h"
#include "graphics/resources/Material.h"
#include "resources/PbrResources.h"
#include "resources/ResourceManager.h"
#include "animation/AnimationPlayer.h"
#include "scene/Scene.h"
#include "platform/Window.h"
#include <array>
#include <cmath>
#include <iostream>

int main()
{
    try
    {
        DirectionalShadowSettings settings;
        settings.halfExtent=3; settings.distance=5; settings.farPlane=15; settings.resolution=256;
        settings.lightMatrix({0,-1,0}); // 正下方灯光不能与默认up向量平行而产生NaN。
        auto bad=settings; bad.resolution=0;
        expectThrow<std::invalid_argument>([&] { bad.validate(); },"Invalid shadow resolution accepted");
        Window window(64,64,"Directional shadow test",false);
        ColorDepthTarget target;
        PbrResources pbr;
        auto surface=pbr.createMaterial({.8f,.8f,.8f,1});
        surface->setCullMode(CullMode::None);
        std::vector<Vertex> vertices={
            {{-2,-2,0},{1,1,1},{0,0},{0,0,1}},{{2,-2,0},{1,1,1},{1,0},{0,0,1}},
            {{2,2,0},{1,1,1},{1,1},{0,0,1}},{{-2,2,0},{1,1,1},{0,1},{0,0,1}}};
        Mesh quad(vertices,{0,1,2,0,2,3});
        auto caster=pbr.createMaterial({.5f,.1f,.1f,1}); caster->setCullMode(CullMode::None);
        Transform plane, block; block.position={-.3f,0,1}; block.scale=glm::vec3(.25f);
        std::vector<RenderItem> items={{&quad,surface.get(),&plane},{&quad,caster.get(),&block}};
        SceneLighting lighting; lighting.mainLight().direction={1,0,-1}; lighting.ambient()=glm::vec3(.03f);
        lighting.mainLight().intensity=3;
        Camera camera; camera.setOrthographic(5,.1f,100);
        Renderer renderer; HdrPipeline hdr; DirectionalShadowMap shadows;
        std::array<unsigned char,64*64*4> bright{}, dark{};
        const auto color=[&](const DirectionalShadowView *view, auto &pixels)
        {
            hdr.render(64,64,{0,0,0,1},[&] { renderer.drawItems(items,camera,1,lighting,view,true); });
            glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        };
        color(nullptr,bright);
        GLint fbo=0,viewport[4]; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo); glGetIntegerv(GL_VIEWPORT,viewport);
        glEnable(GL_SCISSOR_TEST); glScissor(1,1,2,2); glDepthMask(GL_FALSE); glDepthFunc(GL_GREATER);
        auto shadow=shadows.render(items,lighting.mainLight().direction,settings);
        GLint actual=0; GLboolean write=GL_TRUE;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&actual); require(actual==fbo,"Shadow changed caller FBO");
        glGetIntegerv(GL_DEPTH_FUNC,&actual); glGetBooleanv(GL_DEPTH_WRITEMASK,&write);
        require(actual==GL_GREATER && !write && glIsEnabled(GL_SCISSOR_TEST),"Shadow pass did not restore depth/scissor state");
        GLint afterViewport[4]; glGetIntegerv(GL_VIEWPORT,afterViewport);
        require(std::equal(viewport,viewport+4,afterViewport),"Shadow viewport not restored");
        glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);
        color(&shadow,dark);
        int darker=0;
        for (std::size_t i=0;i<bright.size();i+=4) { if (int(bright[i])-int(dark[i])>30) { ++darker; } }
        require(darker>60,"Shadow did not darken visible receiver pixels");

        // Alpha为0的MASK完全不写深度；BLEND不作为不透明遮挡物。
        caster->setRenderMode(RenderMode::AlphaMask); caster->setBaseColor({1,1,1,0});
        shadow=shadows.render(items,lighting.mainLight().direction,settings);
        color(&shadow,dark);
        auto receiverOnly=shadows.render({items.front()},lighting.mainLight().direction,settings);
        color(&receiverOnly,bright);
        require(dark==bright,"Transparent MASK still cast a solid shadow");
        caster->setRenderMode(RenderMode::AlphaBlend); caster->setBaseColor({1,1,1,.4f});
        shadow=shadows.render(items,lighting.mainLight().direction,settings);
        color(&shadow,dark);
        receiverOnly=shadows.render({items.front()},lighting.mainLight().direction,settings);
        color(&receiverOnly,bright);
        require(dark==bright,"BLEND unexpectedly cast opaque shadow");
        // 主灯关闭时仅有环境项，阴影不能把环境光也乘黑。
        caster->setRenderMode(RenderMode::Opaque); lighting.mainLight().intensity=0;
        shadow=shadows.render(items,lighting.mainLight().direction,settings);
        color(&shadow,dark); color(nullptr,bright);
        require(dark==bright,"Shadow incorrectly darkened ambient contribution");
        surface->setShaderOutputsSrgb(true);
        expectThrow<std::invalid_argument>([&] { color(nullptr,bright); },"HDR accepted already-encoded material");
        surface->setShaderOutputsSrgb(false);
        auto invalid=items; invalid.back().mesh=nullptr;
        expectThrow<std::invalid_argument>([&] { shadows.render(invalid,{1,0,-1},settings); },"Invalid shadow item accepted");
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&actual); require(actual==fbo,"Shadow exception changed FBO");

        // 同一蒙皮快照用于颜色和深度；两个确定动画时间必须改变阴影深度。
        ResourceManager resources;
        auto model=resources.loadPbrModel("tests/fixtures/skin/ribbon.glb");
        Scene scene;
        auto instance=ModelInstantiator::instantiate(scene,*model);
        AnimationPlayer player(scene,model,instance); player.play(scene,0);
        settings.resolution=128;
        std::vector<float> depthBefore(128*128),depthAfter(128*128);
        shadow=shadows.render(scene.renderItems(),{0,0,-1},settings);
        GLint active=0,texture=0; glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glBindTexture(GL_TEXTURE_2D,shadow.texture); glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depthBefore.data());
        glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(active);
        player.seek(scene,1);
        shadow=shadows.render(scene.renderItems(),{0,0,-1},settings);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,shadow.texture);
        glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depthAfter.data());
        glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(active);
        int changed=0;
        for (std::size_t i=0;i<depthBefore.size();++i) { if (std::abs(depthBefore[i]-depthAfter[i])>.1f) { ++changed; } }
        require(changed>80,"Skinned shadow remained in bind pose");
        require(glGetError()==GL_NO_ERROR,"Shadow pass produced GL error");
        std::cout << "Directional shadow tests passed: receiver, alpha, ambient, skin, resize and state restoration\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
