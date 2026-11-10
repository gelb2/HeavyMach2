#include "Core/Presentation/Visibility.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace hm2;
int main(){
    const MapCamera camera{{},{},{768,1024}};VisibilityState root;
    const auto world=[&](Vec2 screen){return camera.toWorld(screen);};
    assert(inCamera(root,world({-61,-61}),&camera));
    assert(!inCamera(root,world({-61.01f,0}),&camera));
    assert(inCamera(root,world({828,1084}),&camera));
    assert(!inCamera(root,world({828.01f,100}),&camera));
    assert(!inCamera(root,world({100,1084.01f}),&camera));
    refreshVisibility(root,world({2000,2000}),&camera);assert(root.inScreen); // non-visual root is not culled
    VisibilityState image;image.visual=true;image.bounds={-10,-20,10,20};
    const bool before=image.inScreen;refreshVisibility(image,world({-1000,0}),&camera);assert(before&&!image.inScreen);
    const bool next=image.inScreen;refreshVisibility(image,world({100,100}),&camera);assert(!next&&image.inScreen); // prior atlas gate stays observable
    image.removed=true;refreshVisibility(image,world({-1000,0}),&camera);assert(image.inScreen);
    image.show=false;assert(!inCamera(image,{},nullptr));image.show=true;assert(inCamera(image,{1e8f,1e8f},nullptr));
}
