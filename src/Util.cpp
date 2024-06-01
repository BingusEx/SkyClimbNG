#include "Util.h"

RE::NiAVObject* FindBoneNode(const RE::Actor* a_actorptr, const std::string_view a_nodeName, const bool a_isFirstPerson) {
    if (!a_actorptr->Is3DLoaded()) return nullptr;

    const auto model = a_actorptr->Get3D(a_isFirstPerson);

    if (!model) return nullptr;

    if (const auto node_lookup = model->GetObjectByName(a_nodeName)) return node_lookup;

    // Game lookup failed we try and find it manually
    std::deque<RE::NiAVObject*> queue;
    queue.push_back(model);

    while (!queue.empty()) {
        const auto currentnode = queue.front();
        queue.pop_front();
        try {
            if (currentnode) {
                if (const auto ninode = currentnode->AsNode()) {
                    for (const auto& child : ninode->GetChildren()) {
                        // Bredth first search
                        queue.push_back(child.get());
                        // Depth first search
                        // queue.push_front(child.get());
                    }
                }
                // Do smth
                if (currentnode->name.c_str() == a_nodeName) {
                    return currentnode;
                }
            }
        } catch (const std::overflow_error& e) {
            SKSE::log::warn("Find Bone Overflow: {}", e.what());
        }  // this executes if f() throws std::overflow_error (same type rule)
        catch (const std::runtime_error& e) {
            SKSE::log::warn("Find Bone Underflow: {}", e.what());
        }  // this executes if f() throws std::underflow_error (base class rule)
        catch (const std::exception& e) {
            SKSE::log::warn("Find Bone Exception: {}", e.what());
        }  // this executes if f() throws std::logic_error (base class rule)
        catch (...) {
            SKSE::log::warn("Find Bone Exception Other");
        }
    }

    return nullptr;
}


