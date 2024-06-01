#pragma once

inline void ToggleJumpingInternal(bool a_state) {
    RE::ControlMap::GetSingleton()->ToggleControls(RE::ControlMap::UEFlag::kJumping, a_state);
}

[[nodiscard]] inline RE::NiPoint3 CameraDirInternal() {
    const auto worldCamera = RE::Main::WorldRootCamera();

    if (!worldCamera) return {};

    RE::NiPoint3 output;
    output.x = worldCamera->world.rotate.entry[0][0];
    output.y = worldCamera->world.rotate.entry[1][0];
    output.z = worldCamera->world.rotate.entry[2][0];

    return output;
}

[[nodiscard]] inline float MagnitudeXY(float a_x, float a_y) {
    return sqrt(a_x * a_x + a_y * a_y);
}

[[nodiscard]] RE::NiAVObject* FindBoneNode(const RE::Actor* a_actorptr, std::string_view a_nodeName,bool a_isFirstPerson);

[[nodiscard]] inline float GetModelScale(const RE::Actor* a_actor) {
    if (!a_actor) return 1.0f;

    if (!a_actor->Is3DLoaded()) {
        return 1.0;
    }

    if (const auto model = a_actor->Get3D(false)) {
        return model->local.scale;
    }

    if (const auto first_model = a_actor->Get3D(true)) {
        return first_model->local.scale;
    }

    return 1.0;
}

[[nodiscard]] inline float GetNodeScale(const RE::Actor* a_actor, const std::string_view a_boneName) {
    if (!a_actor) return 1.0f;

    if (const auto Node = FindBoneNode(a_actor, a_boneName, false)) {
        return Node->local.scale;
    }
    if (const auto FPNode = FindBoneNode(a_actor, a_boneName, true)) {
        return FPNode->local.scale;
    }
    return 1.0;
}

[[nodiscard]] inline float GetScale() {

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    float TargetScale = 1.0f;
    TargetScale *= GetModelScale(player);                     // Model scale, Scaling done by game
    TargetScale *= GetNodeScale(player, "NPC");  // NPC bone, Racemenu uses this.
    TargetScale *= GetNodeScale(player, "NPC Root [Root]");  // Child bone of "NPC" some other mods scale this bone instead

    if (TargetScale < 0.15f) TargetScale = 0.15f;
    if (TargetScale > 250.f) TargetScale = 250.f;
    return TargetScale;
}