#pragma once
#include "Core/Types.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <set>
#include <utility>

namespace hm2::test {
// Only test observations. The original game order, inputs and RNG are untouched.
// Files supplies the raw read/exists requests, including cache hits.
template<class Owner,class Files> class ResourceBoundaryLedger {
    Owner& owner_;Files& files_;
    std::set<std::string,std::less<>> owned_;
    std::set<std::string,std::less<>> sharedOwned_;
    std::uint64_t observedEpoch_=0;
    std::size_t initialReads_=0,initialProbes_=0;
    auto names()const{
        std::set<std::string,std::less<>> result;
        for(const auto& sprite:owner_.preparationFrame().sprites){
            result.insert(sprite.image);
            if(!sprite.textureSource.image.empty())result.insert(sprite.textureSource.image);
        }
        return result;
    }
    auto sharedNames()const{
        std::set<std::string,std::less<>> result;
        if constexpr(requires{owner_.globalPreparationFrame();})for(const auto& sprite:owner_.globalPreparationFrame().sprites){result.insert(sprite.image);if(!sprite.textureSource.image.empty())result.insert(sprite.textureSource.image);}
        return result;
    }
public:
    std::uint64_t firstUseReads=0,firstUseProbes=0,preparationActions=0,steadyActions=0,draws=0;
    std::set<std::string,std::less<>> requestedNames;
    ResourceBoundaryLedger(Owner& owner,Files& files):owner_(owner),files_(files),owned_(names()),sharedOwned_(sharedNames()),observedEpoch_(owner.resourceRevision()),initialReads_(files.readRequests.size()),initialProbes_(files.probeRequests.size()){}
    template<class Action> void run(Action&& action){
        const auto before=owner_.resourceRevision();
        if(before!=observedEpoch_){owned_=names();sharedOwned_=sharedNames();}
        const auto readStart=files_.readRequests.size(),probeStart=files_.probeRequests.size();
        std::invoke(std::forward<Action>(action));
        const auto after=owner_.resourceRevision();
        const bool io=files_.readRequests.size()!=readStart||files_.probeRequests.size()!=probeStart;
        if(io){
            assert(after!=before); // Actual preparation must wake the GPU consumer.
            const auto prepared=names(),shared=sharedNames();std::set<std::string,std::less<>> withinAction;
            // Target.png has distinct original World and shared managers:
            // a first AirStrike request may legitimately add the shared owner.
            const auto newlyOwned=[&](const std::string& name){return (!owned_.contains(name)&&!sharedOwned_.contains(name))||(shared.contains(name)&&!sharedOwned_.contains(name));};
            for(std::size_t i=readStart;i<files_.readRequests.size();++i){
                const auto& name=files_.readRequests[i];
                if(!newlyOwned(name)||(!prepared.contains(name)&&!shared.contains(name)))std::cerr<<"UNOWNED_RESOURCE_READ "<<name<<" epoch="<<before<<"->"<<after<<'\n';
                assert(newlyOwned(name)&&(prepared.contains(name)||shared.contains(name)));
                assert(name.ends_with(".png")&&withinAction.insert(name).second);
                requestedNames.insert(name);++firstUseReads;
            }
            for(std::size_t i=probeStart;i<files_.probeRequests.size();++i){
                const auto& [name,present]=files_.probeRequests[i];
                assert(newlyOwned(name));
                // A genuinely absent original file remains nil; no replacement.
                assert(!present||prepared.contains(name)||shared.contains(name));++firstUseProbes;
            }
            ++preparationActions;owned_=prepared;sharedOwned_=shared;
        }else if(after==before)++steadyActions;
        else{owned_=names();sharedOwned_=sharedNames();}
        observedEpoch_=after;
    }
    template<class Draw> Frame draw(Draw&& draw){
        const auto reads=files_.readRequests.size(),probes=files_.probeRequests.size();const auto epoch=owner_.resourceRevision();
        Frame result=std::invoke(std::forward<Draw>(draw));
        assert(files_.readRequests.size()==reads&&files_.probeRequests.size()==probes&&owner_.resourceRevision()==epoch);
        ++draws;return result;
    }
    void report(std::string_view label)const{
        assert(files_.readRequests.size()-initialReads_==firstUseReads&&files_.probeRequests.size()-initialProbes_==firstUseProbes);
        std::cout<<"RESOURCE_BOUNDARIES "<<label<<" preparedReads="<<firstUseReads<<" preparedExists="<<firstUseProbes<<" preparationActions="<<preparationActions<<" steadyIO0="<<steadyActions<<" drawIO0="<<draws<<" names=";
        for(const auto& name:requestedNames)std::cout<<name<<',';
        std::cout<<'\n';
    }
};
}
