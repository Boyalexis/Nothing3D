#include "scene/SceneHistory.h"
#include <cstdio>

int main() {
    int failures=0;
    auto check=[&](bool okay,const char* name) { if (!okay) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); } };
    n3d::Scene scene; n3d::SceneHistory history;
    n3d::ObjectData first; first.name="first";
    const auto id=scene.add(first);
    history.recordApplied({"create",id,{},first,0,0,id});
    auto changed=first; changed.positionMm={-237,500,0}; changed.rotationDegrees={15,30,45};
    scene.update(id,changed); history.recordApplied({"edit",id,first,changed,0,id,id});
    check(history.size()==2 && history.cursor()==2 && !history.canRedo(),"two committed operations");
    check(history.undo(scene)==id && scene.find(id)->data==first,"undo exact object parameters");
    check(history.undo(scene)==0 && scene.objects().empty(),"undo creation restores empty selection");
    check(!history.undo(scene) && history.cursor()==0,"empty undo is no-op");
    check(history.redo(scene)==id && scene.find(id)->data==first,"redo creation restores same identity");
    check(history.redo(scene)==id && scene.find(id)->data==changed,"redo edit restores all fields");
    history.undo(scene);
    history.recordApplied({"no-op",id,first,first,0,id,id});
    check(history.canRedo() && history.size()==2,"no-op preserves redo branch");
    auto branch=first; branch.positionMm.x=1200;
    scene.update(id,branch); history.recordApplied({"branch",id,first,branch,0,id,id});
    check(!history.canRedo() && history.size()==2 && history.undoLabel()=="branch","new edit discards redo branch");
    history.undo(scene); history.undo(scene);
    const auto newer=scene.add(first);
    check(newer>id,"undo never recycles IDs for new creation");
    history.clear();
    check(!history.canUndo() && !history.canRedo(),"clear history");
    auto invalid=first; std::get<n3d::BoxParameters>(invalid.shape).width=-1;
    try { scene.restore({100,invalid},0); check(false,"invalid restore throws"); } catch(const std::invalid_argument&) {}
    check(scene.objects().size()==1 && scene.add(first)==newer+1,"invalid restore preserves objects and ID allocator");
    const auto last=scene.add(first);
    scene.restore({id,first},1);
    check(scene.objects()[1].id==id && scene.objects().back().id==last,"restore preserves list index");
    try { scene.restore({id,first},0); check(false,"duplicate restore throws"); } catch(const std::invalid_argument&) {}
    history.recordApplied({"edit",id,first,branch,0,id,id}); // Deliberately inconsistent history.
    try { history.undo(scene); check(false,"diverged state rejected"); } catch(const std::logic_error&) {}
    check(history.cursor()==1 && scene.find(id)->data==first,"failed replay leaves cursor and data intact");
    if (!failures) std::puts("PASS: history replay, identity, branches, no-op, validation");
    return failures?1:0;
}
