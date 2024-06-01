#include "Plugin.h"
#include "Util.h"

//-------------------
//  Utility
//-------------------

float PlayerScale = 1.0f;

//camera versus head 'to object angle'. Angle between the vectors 'camera to object' and 'player head to object'
float CameraVsHeadToObjectAngle(RE::NiPoint3 a_point) {
    const auto player = RE::PlayerCharacter::GetSingleton();

    if (!player) return 1.0f;

    RE::NiPoint3 playerToObject = a_point - (player->GetPosition() + RE::NiPoint3(0, 0, 120));

    playerToObject /= playerToObject.Length();

    RE::NiPoint3 camDir = CameraDirInternal();

    float dot = playerToObject.Dot(camDir);

    constexpr float radToDeg = 57.2958f;

    return acos(dot) * radToDeg;
}

float RayCast(RE::NiPoint3 a_start, RE::NiPoint3 a_dir, float a_maxDist, RE::hkVector4 &a_normalOut, bool a_logLayer, RE::COL_LAYER a_layerMask) {

    RE::NiPoint3 rayEnd = a_start + a_dir * a_maxDist;

    const auto bhkWorld = RE::PlayerCharacter::GetSingleton()->GetParentCell()->GetbhkWorld();
    if (!bhkWorld) {
        return a_maxDist;
    }

    RE::bhkPickData pickData;

    const auto havokWorldScale = RE::bhkWorld::GetWorldScale();

    pickData.rayInput.from = a_start * havokWorldScale;
    pickData.rayInput.to = rayEnd * havokWorldScale;
    pickData.rayInput.enableShapeCollectionFilter = false;
    pickData.rayInput.filterInfo = RE::bhkCollisionFilter::GetSingleton()->GetNewSystemGroup() << 16 | std::to_underlying(a_layerMask);

    if (bhkWorld->PickObject(pickData); pickData.rayOutput.HasHit()) {

        a_normalOut = pickData.rayOutput.normal;

        uint32_t layerIndex = pickData.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo & 0x7F;

        if (a_logLayer) logger::info("layer hit: {}", layerIndex);

        //fail if hit a character
        switch (static_cast<RE::COL_LAYER>(layerIndex)) {
            case RE::COL_LAYER::kStatic:
            case RE::COL_LAYER::kCollisionBox:
            case RE::COL_LAYER::kTerrain:
            case RE::COL_LAYER::kGround:
            case RE::COL_LAYER::kProps:
            case RE::COL_LAYER::kClutter:

                // hit something useful!
                return a_maxDist * pickData.rayOutput.hitFraction;

            default: {
                return -1;

            } break;
        }

    }

    if (a_logLayer) logger::info("nothing hit");

    a_normalOut = RE::hkVector4(0, 0, 0, 0);

    //didn't hit anything!
    return a_maxDist;
}

//-------------------
//  Main
//-------------------

int LedgeCheck(RE::NiPoint3 &a_ledgePoint, RE::NiPoint3 a_checkDir, float a_minLedgeHeight, float a_maxLedgeHeight) {

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return 0;

    const auto playerPos = player->GetPosition();


    float startZOffset = 100;  // how high to start the raycast above the feet of the player
    float playerHeight = 120 * PlayerScale;  // how much headroom is needed
    float minUpCheck = 100;    // how low can the roof be relative to the raycast starting point?
    float maxUpCheck = (a_maxLedgeHeight - startZOffset) + 20 * PlayerScale;  // how high do we even check for a roof?
    float fwdCheck = 10; // how much each incremental forward check steps forward
    int fwdCheckIterations = 15;  // how many incremental forward checks do we make?
    float minLedgeFlatness = 0.7f;  // 1 is perfectly flat, 0 is completely perpendicular

    RE::hkVector4 normalOut(0, 0, 0, 0);

    // raycast upwards to sky, making sure no roof is too low above us
    RE::NiPoint3 upRayStart;
    upRayStart.x = playerPos.x;
    upRayStart.y = playerPos.y;
    upRayStart.z = playerPos.z + startZOffset;

    RE::NiPoint3 upRayDir(0, 0, 1);

    float upRayDist = RayCast(upRayStart, upRayDir, maxUpCheck, normalOut, false, RE::COL_LAYER::kLOS);

    if (upRayDist < minUpCheck) {
        return -1;
    }

    RE::NiPoint3 fwdRayStart = upRayStart + upRayDir * (upRayDist - 10);

    RE::NiPoint3 downRayDir(0, 0, -1);

    bool foundLedge = false;

    // if nothing above, raycast forwards then down to find ledge
    // incrementally step forward to find closest ledge in front
    for (int i = 0; i < fwdCheckIterations; i++) {
        // raycast forward

        float fwdRayDist = RayCast(fwdRayStart, a_checkDir, fwdCheck * static_cast<float>(i), normalOut, false, RE::COL_LAYER::kLOS);

        if (fwdRayDist < fwdCheck * static_cast<float>(i)) {
            continue;
        }

        // if nothing forward, raycast back down

        RE::NiPoint3 downRayStart = fwdRayStart + a_checkDir * fwdRayDist;

        float downRayDist = RayCast(downRayStart, downRayDir, startZOffset + maxUpCheck, normalOut, false, RE::COL_LAYER::kLOS);

        a_ledgePoint = downRayStart + downRayDir * downRayDist;

        float normalZ = normalOut.quad.m128_f32[2];

        // if found ledgePoint is too low/high, or the normal is too steep, or the ray hit oddly soon, skip and
        // increment forward again
        if (a_ledgePoint.z < playerPos.z + a_minLedgeHeight || a_ledgePoint.z > playerPos.z + a_maxLedgeHeight ||
            downRayDist < 10 || normalZ < minLedgeFlatness) {
            continue;
        }
        foundLedge = true;
        break;
    }

    // if no ledge found, return false
    if (foundLedge == false) {
        return -1;
    }

    // make sure player can stand on top
    float headroomBuffer = 10;

    RE::NiPoint3 headroomRayStart = a_ledgePoint + upRayDir * headroomBuffer;

    float headroomRayDist = RayCast(headroomRayStart, upRayDir, playerHeight - headroomBuffer, normalOut, false, RE::COL_LAYER::kLOS);

    if (headroomRayDist < playerHeight - headroomBuffer) {
        return -1;
    }

    if (a_ledgePoint.z - playerPos.z < 175 * PlayerScale) {
        return 1;
    }
    return 2;
}

int VaultCheck(RE::NiPoint3 &a_ledgePoint, RE::NiPoint3 a_checkDir, float a_vaultLength, float a_maxElevationIncrease, float a_minVaultHeight, float a_maxVaultHeight) {

    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto playerPos = player->GetPosition();

    RE::hkVector4 normalOut(0, 0, 0, 0);

    float headHeight = 120 * PlayerScale;

    RE::NiPoint3 fwdRayStart;
    fwdRayStart.x = playerPos.x;
    fwdRayStart.y = playerPos.y;
    fwdRayStart.z = playerPos.z + headHeight;

    float fwdRayDist = RayCast(fwdRayStart, a_checkDir, a_vaultLength, normalOut, false, RE::COL_LAYER::kLOS);

    if (fwdRayDist < a_vaultLength) {
        return -1;
    }

    int downIterations = static_cast<int>(std::floor(a_vaultLength / 5.0f));

    RE::NiPoint3 downRayDir(0, 0, -1);

    bool foundVaulter = false;
    float foundVaultHeight = -10000;

    bool foundLanding = false;
    float foundLandingHeight = 10000;

    for (int i = 0; i < downIterations; i++) {

        float iDist = static_cast<float>(i) * 5;
        
        RE::NiPoint3 downRayStart = playerPos + a_checkDir * iDist;
        downRayStart.z = fwdRayStart.z;


        float downRayDist = RayCast(downRayStart, downRayDir, headHeight + 100, normalOut, false, RE::COL_LAYER::kLOS);

        float hitHeight = (fwdRayStart.z - downRayDist) - playerPos.z;

        if (hitHeight > a_maxVaultHeight) {
            return -1;
        }
        else if (hitHeight > a_minVaultHeight && hitHeight < a_maxVaultHeight) {
            if (hitHeight >= foundVaultHeight) {
                foundVaultHeight = hitHeight;
                foundLanding = false;
            }
            a_ledgePoint = downRayStart + downRayDir * downRayDist;
            foundVaulter = true;
        } 
        else if (foundVaulter && hitHeight < a_minVaultHeight) {
            if (hitHeight < foundLandingHeight || foundLanding == false) {
                foundLandingHeight = hitHeight;
            }
            foundLandingHeight = std::min(hitHeight, foundLandingHeight);
            foundLanding = true;
        }
        

    }

    if (foundVaulter && foundLanding && foundLandingHeight < a_maxElevationIncrease) {
        a_ledgePoint.z = playerPos.z + foundVaultHeight;

        if (foundLandingHeight < -10) {
            return 4;
        }
        return 3;
    }

    return -1;
}

int GetLedgePoint(RE::TESObjectREFR *vaultMarkerRef, RE::TESObjectREFR *medMarkerRef, RE::TESObjectREFR *highMarkerRef, RE::TESObjectREFR *indicatorRef, bool enableVaulting, bool enableLedges) { 
    const auto player = RE::PlayerCharacter::GetSingleton();

    RE::NiPoint3 cameraDir = CameraDirInternal();

    float cameraDirTotal = MagnitudeXY(cameraDir.x, cameraDir.y);
    RE::NiPoint3 cameraDirFlat;
    cameraDirFlat.x = cameraDir.x / cameraDirTotal;
    cameraDirFlat.y = cameraDir.y / cameraDirTotal;
    cameraDirFlat.z = 0;

    int selectedLedgeType = -1;
    RE::NiPoint3 ledgePoint;

    if (enableLedges) {
        selectedLedgeType = LedgeCheck(ledgePoint, cameraDirFlat, 110 * PlayerScale, 250 * PlayerScale);
    }

    if (selectedLedgeType == -1) {

        if (enableVaulting) {
            selectedLedgeType = VaultCheck(ledgePoint, cameraDirFlat, 120, 10, 50 * PlayerScale, 100 * PlayerScale);
        }

        if (selectedLedgeType == -1)
        {
            return -1;
        }
    }

    // rotate to face camera
    float zAngle = atan2(cameraDirFlat.x, cameraDirFlat.y);

    //it seems we have to MoveTo first in order to get the references into the same cell

    if (indicatorRef->GetParentCell() != player->GetParentCell()){
        indicatorRef->MoveTo(player->AsReference());
    }

    indicatorRef->data.location = ledgePoint + RE::NiPoint3(0,0,5);
    indicatorRef->Update3DPosition(true);

    indicatorRef->data.angle = RE::NiPoint3(0, 0, zAngle);

    RE::TESObjectREFR *ledgeMarker;

    float zAdjust;
    float toCameraAdjust;


    //pick a ledge type

    if (selectedLedgeType == 1) {
        ledgeMarker = medMarkerRef;
        zAdjust = -155 * PlayerScale;
        toCameraAdjust = -50;
    } 
    else if (selectedLedgeType == 2) {
        ledgeMarker = highMarkerRef;
        zAdjust = -200 * PlayerScale;
        toCameraAdjust = -50;
    }
    else {
        ledgeMarker = vaultMarkerRef;
        zAdjust = -60 * PlayerScale;
        toCameraAdjust = -80;
    }

    // adjust the EVG marker for correct positioning
    RE::NiPoint3 adjustedPos;
    adjustedPos.x = ledgePoint.x + cameraDirFlat.x * toCameraAdjust;
    adjustedPos.y = ledgePoint.y + cameraDirFlat.y * toCameraAdjust;
    adjustedPos.z = ledgePoint.z + zAdjust + (12 * PlayerScale);

    if (ledgeMarker->GetParentCell() != player->GetParentCell()) {
        ledgeMarker->MoveTo(player->AsReference());
    }

    ledgeMarker->SetPosition(adjustedPos);

    ledgeMarker->data.angle = RE::NiPoint3(0, 0, zAngle);

    return selectedLedgeType;

}

//-------------------
//  Character State
//-------------------

bool PlayerIsGrounded() {
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    const auto playerPos = player->GetPosition();

    RE::hkVector4 normalOut(0, 0, 0, 0);

    // grounded check
    float groundedCheckDist = 128 + 20;

    RE::NiPoint3 groundedRayStart;
    groundedRayStart.x = playerPos.x;
    groundedRayStart.y = playerPos.y;
    groundedRayStart.z = playerPos.z + 128;

    RE::NiPoint3 groundedRayDir(0, 0, -1);

    float groundedRayDist =
        RayCast(groundedRayStart, groundedRayDir, groundedCheckDist, normalOut, false, RE::COL_LAYER::kLOS);

    if (groundedRayDist == groundedCheckDist || groundedRayDist == -1) {
        return false;
    }

    return true;
}

bool PlayerIsInWater() {
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (auto actorState = player->AsActorState()) {
        if (actorState->IsSwimming()) {
            return true;
        }
    }
    return false;
}


//-------------------
//  VM Functions
//-------------------

std::string SayHello(RE::StaticFunctionTag *) {
    return fmt::format("Skyclimb {}.{}.{}", Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());
}

void ToggleJumping(RE::StaticFunctionTag *, bool enabled) {
    ToggleJumpingInternal(enabled);
}

void EndAnimationEarly(RE::StaticFunctionTag *, RE::TESObjectREFR *objectRef) {
    objectRef->NotifyAnimationGraph("IdleFurnitureExit");
}

int UpdateParkourPoint(RE::StaticFunctionTag *, RE::TESObjectREFR *vaultMarkerRef, RE::TESObjectREFR *medMarkerRef, RE::TESObjectREFR *highMarkerRef, RE::TESObjectREFR *indicatorRef, bool useJumpKey, bool enableVaulting, bool enableLedges) {

    if (PlayerIsGrounded() == false || PlayerIsInWater() == true) {
        return -1;
    }


    PlayerScale = GetScale();
    int foundLedgeType = GetLedgePoint(vaultMarkerRef, medMarkerRef, highMarkerRef, indicatorRef, enableVaulting, enableLedges);

    if (useJumpKey) {
        if (foundLedgeType >= 0) {
            ToggleJumpingInternal(false);
        } else {
            ToggleJumpingInternal(true);
        }
    }

    return foundLedgeType;
}

//-------------------
//  Init
//-------------------

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");

    auto pluginName = "SkyClimb";
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

    set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
}

bool RegisterPapyrus(RE::BSScript::IVirtualMachine * vm) { 
    vm->RegisterFunction("SayHello", "SkyClimbPapyrus", SayHello);
    vm->RegisterFunction("ToggleJumping", "SkyClimbPapyrus", ToggleJumping);
    vm->RegisterFunction("EndAnimationEarly", "SkyClimbPapyrus", EndAnimationEarly);
    vm->RegisterFunction("UpdateParkourPoint", "SkyClimbPapyrus", UpdateParkourPoint);

    return true; 
}

//-------------------
//  SKSE Exports
//-------------------

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = [] {
    SKSE::PluginVersionData v;
    v.PluginVersion(Plugin::VERSION);
    v.PluginName("SkyClimb");
    v.AuthorName("Sokco");
    v.UsesAddressLibrary(true);
    v.CompatibleVersions({SKSE::RUNTIME_SSE_LATEST});
    v.HasNoStructUse(true);
    return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface *a_skse) {
    SetupLog();
    Init(a_skse);
    bool res = SKSE::GetPapyrusInterface()->Register(RegisterPapyrus);
    logger::info(FMT_STRING("Papyrus Registered: {}"), res);
    return res;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface *a_skse, SKSE::PluginInfo *a_info) {
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = Plugin::NAME.data();
    a_info->version = Plugin::VERSION.pack();

    if (a_skse->IsEditor()) {
        // logger::critical("Loaded in editor, marking as incompatible"sv);
        return false;
    }

    const auto ver = a_skse->RuntimeVersion();
    if (ver < SKSE::RUNTIME_SSE_1_5_97) {
        logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
        return false;
    }
    return true;
}