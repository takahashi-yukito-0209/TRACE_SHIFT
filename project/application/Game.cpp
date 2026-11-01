#include "Game.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "InputManager.h"
#include "Logger.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "StringUtility.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "application/scenes/SceneFactory.h"
#include "application/scenes/StageSelectionState.h"
#include "engine/base/SceneManager.h"
#include "engine/base/PostProcess.h"
#include "engine/base/RenderTarget.h"
#include "engine/base/SrvManager.h"
#include "engine/debug/DebugRenderer.h"
#include "engine/sound/Sound.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "mathUtility.h"
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <d3d12.h>
#include <dbghelp.h>
#include <dinput.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <vector>
#include <wrl.h>
#include <xaudio2.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dinput8.lib")

using namespace MyEngine;
using namespace Math;

namespace {
constexpr const char* kInitialSceneName = "Title"; // 起動時にデモ付きタイトルを表示するシーン名
constexpr Vector3 kInitialCameraRotate = { 5.9f, -7.43f, 0.0f };
constexpr Vector3 kInitialCameraTranslate = { 0.0f, 1.0f, -18.0f };
constexpr Vector4 kWhiteColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色
constexpr Vector4 kPointLightPosition = { 0.0f, 1.5f, 0.0f, 0.0f };
constexpr Vector4 kPointLightColor = { 1.0f, 1.0f, 1.0f, 1.5f };
constexpr float kPointLightRadius = 6.0f;
constexpr float kLightDecay = 2.0f;
constexpr Vector4 kSpotLightPosition = { 2.0f, 1.25f, -3.0f, 0.0f };
constexpr Vector4 kSpotLightColor = { 1.0f, 1.0f, 1.0f, 2.0f };
constexpr float kSpotLightDistance = 7.0f;
constexpr Vector3 kSpotLightDirection = { -1.0f, -1.0f, 0.0f };
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSpotLightAngle = kPi / 3.0f;
constexpr float kSpotLightFalloffStartAngle = kPi / 2.0f;
constexpr float kInitialDebugCameraWidth = 1280.0f;
constexpr float kInitialDebugCameraHeight = 720.0f;
constexpr float kDefaultCameraRotateSpeed = 0.01f;
constexpr float kDefaultCameraZoomSpeed = 0.05f;
constexpr int kMouseButtonLeft = 0; // 左マウスボタン番号
constexpr int kMouseButtonMiddle = 2; // 中マウスボタン番号
constexpr float kFixedDeltaTime = 1.0f / 60.0f;
constexpr float kSceneFadeOutDuration = 0.55f; // 中央への収束から暗転までの時間
constexpr float kSceneFadeInDuration = 0.65f; // 歪みをほどいて色を戻す時間
constexpr float kSceneFadeOutDarkenStart = 0.25f; // 収束が見えてから黒を重ね始める時間率
constexpr float kSceneTransitionContraction = 0.18f; // 暗転直前に中心へ縮める画面の割合
constexpr float kSceneTransitionExpansion = 0.08f; // 明転開始時に残す画面の縮小割合
constexpr float kSceneTransitionDistortionStrength = 0.035f; // 暗転直前の画面歪み強度
constexpr float kSceneTransitionBlurWidth = 0.012f; // 暗転直前の放射状ブラー幅
enum class SceneFadePhase { Idle, FadeOut, Black, Prepare, FadeIn }; // 切替後の描画準備まで黒で隠す遷移段階
constexpr size_t kLogDateBufferSize = 32;
constexpr size_t kDebugCameraLogBufferSize = 128;
}

Game::Game()
    : impl_(nullptr)
{
}


struct Game::Impl {

    bool initialized = false;
    bool endRequested = false;


    Transform transform = {};

    WinApp winApp;
    HWND hwnd = nullptr;
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    SoundSystem soundSystem;
    std::shared_ptr<SoundClip> soundData1;

    std::unique_ptr<SpriteCommon> spriteCommon;
    SrvManager srvManager;
    std::unique_ptr<Object3dCommon> object3dCommon;

    std::unique_ptr<DebugRenderer> debugRenderer; // デバッグ描画管理
    std::unique_ptr<Camera> camera;
    std::vector<std::unique_ptr<Sprite>> sprites;
    std::vector<std::unique_ptr<Object3d>> objects3d;
    std::unique_ptr<Object3d> particlePlane;

    std::unique_ptr<SceneManager> sceneManager;
    std::shared_ptr<GameApp::StageSelectionState> stageSelection = std::make_shared<GameApp::StageSelectionState>(); // シーン切替後も保持する選択位置とクリア履歴

    ParticleEmitter pmEmitter;

    DebugCamera debugCamera;
    bool isDebugCameraControl = true;
    bool useDebugCameraForRender = false;

    ImGuiManager imguiManager;

    std::string pendingSceneName;
    std::unique_ptr<Sprite> sceneFadeSprite; // シーン破棄の影響を受けない画像なしの暗転矩形
    SceneFadePhase sceneFadePhase = SceneFadePhase::Idle; // シーン遷移の現在段階
    float sceneFadeTime = 0.0f; // 現在のフェード段階で経過した時間
    float sceneFadeAlpha = 0.0f; // 共通暗転矩形の不透明度
    bool blackFrameDrawn = false; // 完全な黒を一度描画してから切り替えるための確認値
    RenderTarget sceneTransitionTarget; // UIを含む最終画面を収集する描画先
    RenderTarget sceneTransitionIntermediateTarget; // 歪みからブラーへ渡す描画先
    PostProcess sceneTransitionPostProcess; // ゲーム内時間演出から独立した遷移用設定
    bool sceneTransitionCaptureActive = false; // 今回の描画で画面を収集中か
    int sceneTransitionOutputHandle = -1; // 演出結果を戻す描画先（-1はバックバッファ）
};

/// <summary>
/// Game クラスのデストラクタ。
/// </summary>
Game::~Game()
{
}

/// <summary>
/// クラッシュダンプ出力を設定する。
/// </summary>
void Game::SetupCrashDumpHandler()
{
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* exception) -> LONG {
        SYSTEMTIME time;
        GetLocalTime(&time);
        wchar_t filePath[MAX_PATH] = { 0 };
        CreateDirectory(L"./Dumps", nullptr);
        StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
        HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
        DWORD processId = GetCurrentProcessId();
        DWORD threadId = GetCurrentThreadId();
        MINIDUMP_EXCEPTION_INFORMATION minidumpInformation { 0 };
        minidumpInformation.ThreadId = threadId;
        minidumpInformation.ExceptionPointers = exception;
        minidumpInformation.ClientPointers = TRUE;
        MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
        CloseHandle(dumpFileHandle);
        return EXCEPTION_EXECUTE_HANDLER;
    });
}

/// <summary>
/// ログファイル出力を設定する。
/// </summary>
void Game::SetupLogFile()
{
    std::filesystem::create_directory("logs");
    std::time_t now_c = std::time(nullptr);
    struct tm local_tm;
    localtime_s(&local_tm, &now_c);
    char dateBuf[kLogDateBufferSize];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d_%H%M%S", &local_tm);
    std::string dateString(dateBuf);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    Logger::SetLogFile(logFilePath);
    Logger::SetErrorLogFile(logFilePath);
}

/// <summary>
/// ウィンドウと入力を初期化する。
/// </summary>
bool Game::InitializeWindowAndInput(HINSTANCE hInstance, int nCmdShow)
{
#ifdef _DEBUG
    constexpr wchar_t kWindowTitle[] = L"TRACE SHIFT - Debug Build"; // Debug版のウィンドウタイトル
#elif defined(USE_IMGUI)
    constexpr wchar_t kWindowTitle[] = L"TRACE SHIFT - Development Build"; // Development版のウィンドウタイトル
#else
    constexpr wchar_t kWindowTitle[] = L"TRACE SHIFT"; // Release版のウィンドウタイトル
#endif
    impl_->winApp.Initialize(hInstance, nCmdShow, kWindowTitle);
    impl_->hwnd = impl_->winApp.GetHwnd();

#ifdef USE_IMGUI
    impl_->winApp.SetCursorVisible(true);
#else
    impl_->winApp.SetCursorVisible(false);
#endif

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(FALSE);
    }

#endif

    HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(impl_->directInput.GetAddressOf()), nullptr);
    if (FAILED(result)) {
        Logger::Log("Error: DirectInput8Create failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    if (!InputManager::GetInstance()->Initialize(impl_->directInput.Get(), impl_->hwnd)) {
        Logger::Log("Error: InputManager::Initialize failed.\n");
        impl_->winApp.Finalize();
        return false;
    }

    return true;
}

/// <summary>
/// エンジン共通リソースを初期化する。
/// </summary>
bool Game::InitializeEngineResources(HINSTANCE hInstance)
{
    std::unique_ptr<SpriteCommon> spriteCommonTmp;
    std::unique_ptr<Object3dCommon> object3dCommonTmp;

    if (!Framework::InitializeEngine(hInstance, &impl_->winApp, impl_->hwnd,
            spriteCommonTmp, impl_->srvManager, object3dCommonTmp)) {
        Logger::Log("Error: Framework::InitializeEngine failed\n");
        impl_->winApp.Finalize();
        return false;
    }


    impl_->spriteCommon = std::move(spriteCommonTmp);
    impl_->object3dCommon = std::move(object3dCommonTmp);
    impl_->debugRenderer = std::make_unique<DebugRenderer>();
    impl_->debugRenderer->Initialize(DirectXCommon::GetInstance());

    return true;
}

/// <summary>
/// カメラとライトを初期化する。
/// </summary>
void Game::InitializeCameraAndLighting()
{
    impl_->camera = std::make_unique<Camera>();
    impl_->camera->SetRotate(kInitialCameraRotate);
    impl_->camera->SetTranslate(kInitialCameraTranslate);
    impl_->camera->Update();
    impl_->object3dCommon->SetDefaultCamera(impl_->camera.get());

    Object3d::DirectionalLight* directionalLightData = impl_->object3dCommon->GetDirectionalLightData();
    if (directionalLightData) {
        directionalLightData->intensity = kWhiteColor.w;
        directionalLightData->color = kWhiteColor;
        directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
    }

    if (impl_->object3dCommon) {
        Object3d::PointLight pl = {};
        pl.position = kPointLightPosition;
        pl.color = kPointLightColor;
        pl.radius = kPointLightRadius;
        pl.decay = kLightDecay;
        pl.enabled = 1;
        impl_->object3dCommon->AddPointLight(pl);
    }

    if (impl_->object3dCommon) {
        auto sl = impl_->object3dCommon->GetSpotLightData();
        if (sl) {
            sl->position = kSpotLightPosition;
            sl->color = kSpotLightColor;
            sl->distance = kSpotLightDistance;
            sl->direction = MathUtil::Normalize(kSpotLightDirection);
            sl->decay = kLightDecay;
            sl->cosAngle = cosf(kSpotLightAngle);
            sl->cosFalloffStart = cosf(kSpotLightFalloffStartAngle);
            sl->enabled = 1;
        }
    }

    impl_->debugCamera.Initialize(kInitialDebugCameraWidth, kInitialDebugCameraHeight);

    if (impl_->object3dCommon) {
        impl_->object3dCommon->SetDebugCamera(&impl_->debugCamera);
        impl_->object3dCommon->SetUseDebugCameraForRender(impl_->useDebugCameraForRender);
        impl_->object3dCommon->SetEnableDebugCameraInput(impl_->isDebugCameraControl);
    }
}

/// <summary>
/// デバッグ機能、ImGui、サウンドを初期化する。
/// </summary>
void Game::InitializeDebugToolsAndSound()
{
    impl_->imguiManager.Initialize(impl_->hwnd, &impl_->srvManager);

    if (impl_->soundSystem.Initialize()) {
        impl_->soundData1 = impl_->soundSystem.LoadFromFile("resources/mokugyo.wav");
    } else {
        Logger::Warn("Game::Initialize: sound system initialization failed.\n");
    }
}

/// <summary>
/// リサイズ通知を設定する。
/// </summary>
void Game::SetupResizeCallbacks()
{
    DirectXCommon::GetInstance()->SetOnResizeCallback([this](uint32_t w, uint32_t h) {
        if (impl_->camera) {
            float aspect = (h != 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
            impl_->camera->SetAspectRatio(aspect);
            impl_->camera->Update();
        }
        impl_->debugCamera.Initialize(static_cast<float>(w), static_cast<float>(h));
        if (impl_->sceneManager) {
            impl_->sceneManager->OnWindowResize(w, h);
        }
    });

    impl_->winApp.SetResizeCallback([](uint32_t width, uint32_t height) {
        DirectXCommon::GetInstance()->OnWindowResize(width, height);
    });
}

/// <summary>
/// シーン管理を初期化する。
/// </summary>
void Game::InitializeScene()
{
    impl_->sceneFadeSprite = std::make_unique<Sprite>();
    impl_->sceneFadeSprite->InitializeSolidColor(impl_->spriteCommon.get(), &impl_->srvManager);
    DirectXCommon* directXCommon = DirectXCommon::GetInstance(); // 共通遷移リソースの作成先
    RenderTargetDesc transitionDesc {}; // ウィンドウサイズへ追従する遷移用描画設定
    transitionDesc.width = static_cast<uint32_t>(directXCommon->GetRenderWidth());
    transitionDesc.height = static_cast<uint32_t>(directXCommon->GetRenderHeight());
    transitionDesc.format = directXCommon->GetSwapChainFormat();
    transitionDesc.resizeWithWindow = true;
    transitionDesc.useDepth = true;
    impl_->sceneTransitionTarget.Initialize(directXCommon, transitionDesc);
    transitionDesc.useDepth = false;
    impl_->sceneTransitionIntermediateTarget.Initialize(directXCommon, transitionDesc);
    impl_->sceneTransitionPostProcess.Initialize(directXCommon);
    impl_->sceneTransitionPostProcess.SetDistortionCenter({ 0.5f, 0.5f });
    impl_->sceneTransitionPostProcess.SetDistortionRadius(1.2f);
    impl_->sceneTransitionPostProcess.SetDistortionWaveCount(2.0f);
    impl_->sceneTransitionPostProcess.SetRadialBlurCenter({ 0.5f, 0.5f });
    impl_->sceneTransitionPostProcess.SetRadialBlurSampleCount(12u);
    impl_->sceneManager = std::make_unique<SceneManager>();
    impl_->sceneManager->Initialize();
    SetupResizeCallbacks();

    SceneContext sctx;
    sctx.object3dCommon = impl_->object3dCommon.get();
    sctx.spriteCommon = impl_->spriteCommon.get();
    sctx.camera = impl_->camera.get();
    sctx.particleManager = ParticleManager::GetInstance();
    sctx.textureManager = TextureManager::GetInstance();
    sctx.srvManager = &impl_->srvManager;
    sctx.directXCommon = DirectXCommon::GetInstance();
    sctx.debugRenderer = impl_->debugRenderer.get();
    sctx.imguiManager = &impl_->imguiManager;
    sctx.soundSystem = &impl_->soundSystem;
    sctx.requestSceneChange = [this](const std::string& sceneName) {
        RequestSceneChange(sceneName);
    };
    sctx.drawSceneTransition = [this]() { DrawSceneTransition(); };
    sctx.isSceneTransitionActive = [this]() { return impl_->sceneFadePhase != SceneFadePhase::Idle; };
    sctx.beginSceneTransition = [this](int outputTargetHandle) { BeginSceneTransition(outputTargetHandle); };
    impl_->sceneManager->SetContext(sctx);

    auto initial = GameApp::SceneFactory::Create(kInitialSceneName, impl_->stageSelection);
    if (initial) {
        impl_->sceneManager->ChangeScene(std::move(initial));
    }
}

/// <summary>
/// ゲームの初期化処理を行う。
/// </summary>
bool Game::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    if (!Framework::Initialize(hInstance, nCmdShow)) {
        return false;
    }

    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    SetupCrashDumpHandler();
    SetupLogFile();

    if (!InitializeWindowAndInput(hInstance, nCmdShow)) {
        return false;
    }

    if (!InitializeEngineResources(hInstance)) {
        return false;
    }

    InitializeCameraAndLighting();
    InitializeDebugToolsAndSound();

    ParticleManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(),
        impl_->object3dCommon.get(),
        &impl_->srvManager,
        TextureManager::GetInstance(),
        &impl_->imguiManager);

    impl_->initialized = true;
    impl_->endRequested = false;

    InitializeScene();

    return true;
}

/// <summary>
/// ゲームの更新処理を行う。
/// </summary>
void Game::Update()
{
    Framework::Update();

    if (!impl_ || !impl_->initialized) {
        return;
    }

    if (!impl_->winApp.ProcessMessage()) {
        impl_->endRequested = true;
        return;
    }

    InputManager::GetInstance()->Update();

    bool debugInputEnabled = impl_->object3dCommon ? impl_->object3dCommon->GetEnableDebugCameraInput() : impl_->isDebugCameraControl;
    if (debugInputEnabled) {
        auto* input = InputManager::GetInstance();
        const long deltaX = input->GetMouseDeltaX();
        const long deltaY = input->GetMouseDeltaY();
        const long wheelDelta = input->GetMouseDeltaZ();

#ifdef USE_IMGUI
        const bool canUseCameraInput = !impl_->imguiManager.IsCapturingInput() || impl_->imguiManager.IsSceneViewHovered();
#else
        const bool canUseCameraInput = true;
#endif
        const bool isLeftDragging = canUseCameraInput && input->IsMouseButtonPressed(kMouseButtonLeft) && (deltaX != 0 || deltaY != 0);
        const bool isWheelMoving = canUseCameraInput && wheelDelta != 0; // ホイールによるカメラ移動があるか
        const bool isMiddleButtonPressed = input->IsMouseButtonPressed(kMouseButtonMiddle); // 中ボタン押し込み中か
        const bool useDebugRender = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;

        if (useDebugRender) {
            if (isLeftDragging) {
                impl_->debugCamera.OnMouseDrag(static_cast<float>(deltaX), static_cast<float>(deltaY));
            }
            if (isWheelMoving) {
                if (isMiddleButtonPressed) {
                    Vector3 cameraTranslate = impl_->debugCamera.GetTranslation(); // デバッグカメラの現在位置
                    cameraTranslate.y += static_cast<float>(wheelDelta) * kDefaultCameraZoomSpeed;
                    impl_->debugCamera.SetTranslation(cameraTranslate);
                } else {
                    impl_->debugCamera.OnMouseWheel(static_cast<float>(wheelDelta));
                }
            }
            impl_->debugCamera.Update();
        } else if (impl_->camera) {
            bool cameraChanged = false;
            if (isLeftDragging) {
                Vector3 cameraRotate = impl_->camera->GetRotate();
                cameraRotate.y += static_cast<float>(deltaX) * kDefaultCameraRotateSpeed;
                cameraRotate.x += static_cast<float>(deltaY) * kDefaultCameraRotateSpeed;
                impl_->camera->SetRotate(cameraRotate);
                cameraChanged = true;
            }
            if (isWheelMoving) {
                Vector3 cameraTranslate = impl_->camera->GetTranslate(); // 通常カメラの現在位置
                if (isMiddleButtonPressed) {
                    cameraTranslate.y += static_cast<float>(wheelDelta) * kDefaultCameraZoomSpeed;
                } else {
                    cameraTranslate.z += static_cast<float>(wheelDelta) * kDefaultCameraZoomSpeed;
                }
                impl_->camera->SetTranslate(cameraTranslate);
                cameraChanged = true;
            }
            if (cameraChanged) {
                impl_->camera->Update();
            }
        }
    }

    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_SPACE) && !InputManager::GetInstance()->IsKeyJustReleased(DIK_SPACE)) {
        impl_->soundSystem.Play(impl_->soundData1);
    }

    const float dt = kFixedDeltaTime;
    impl_->soundSystem.Poll();

    Matrix4x4 worldMatrix = MathUtil::MakeAffineMatrix(impl_->transform.scale, impl_->transform.rotate, impl_->transform.translate);
    Matrix4x4 viewMatrix;
    Matrix4x4 projectionMatrix;

    if (InputManager::GetInstance()->IsKeyJustPressed(DIK_F1)) {
        if (impl_->object3dCommon) {
            bool cur = impl_->object3dCommon->GetUseDebugCameraForRender();
            impl_->object3dCommon->SetUseDebugCameraForRender(!cur);
            char buf[kDebugCameraLogBufferSize];
            sprintf_s(buf, "Debug camera for render: %s\n", !cur ? "ON" : "OFF");
            Logger::Log(buf);
        } else {
            impl_->useDebugCameraForRender = !impl_->useDebugCameraForRender;
            char buf[kDebugCameraLogBufferSize];
            sprintf_s(buf, "Debug camera for render: %s\n", impl_->useDebugCameraForRender ? "ON" : "OFF");
            Logger::Log(buf);
        }
    }

    bool useDebugForRender = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;
    if (useDebugForRender) {
        impl_->debugCamera.Update();
        viewMatrix = impl_->debugCamera.GetViewMatrix();
        projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
    } else {
        if (impl_->camera) {
            impl_->camera->Update();
            viewMatrix = impl_->camera->GetViewMatrix();
            projectionMatrix = impl_->camera->GetProjectionMatrix();
        } else {
            impl_->debugCamera.Update();
            viewMatrix = impl_->debugCamera.GetViewMatrix();
            projectionMatrix = impl_->debugCamera.GetProjectionMatrix();
        }
    }

    Matrix4x4 worldViewProjectionMatrix = MathUtil::Multiply(worldMatrix, MathUtil::Multiply(viewMatrix, projectionMatrix));

    if (impl_->object3dCommon) {
        auto cam = impl_->object3dCommon->GetCameraData();
        if (cam) {
            if (useDebugForRender) {
                cam->worldPosition = impl_->debugCamera.GetTranslation();
            } else if (impl_->camera) {
                cam->worldPosition = impl_->camera->GetTranslate();
            }
            cam->view = viewMatrix;
        }
    }

    for (auto& obj : impl_->objects3d) {
        if (obj) {
            obj->Update(viewMatrix, projectionMatrix);
        }
    }

    for (uint32_t i = 0; i < impl_->sprites.size(); i++) {
        if (impl_->sprites[i]) {
            impl_->sprites[i]->Update();
        }
    }

    UpdateSceneTransition(dt);
    if (impl_->sceneManager && (impl_->sceneFadePhase == SceneFadePhase::Idle || impl_->sceneFadePhase == SceneFadePhase::FadeIn)) {
        impl_->sceneManager->Update(dt);
    }
}

/// <summary>
/// 重複要求を防ぎ、シーン切り替え前の暗転を開始する。
/// </summary>
void Game::RequestSceneChange(const std::string& sceneName)
{
    if (sceneName == "Play" && impl_->stageSelection &&
        !impl_->stageSelection->IsStageUnlocked(impl_->stageSelection->selectedStageIndex)) {
        return;
    }
    if (!GameApp::SceneFactory::IsSceneAvailable(sceneName) || !impl_->sceneManager || impl_->sceneFadePhase != SceneFadePhase::Idle ||
        impl_->sceneManager->GetCurrentSceneName() == sceneName) {
        return;
    }
    impl_->pendingSceneName = sceneName;
    impl_->sceneFadePhase = SceneFadePhase::FadeOut;
    impl_->sceneFadeTime = 0.0f;
    impl_->blackFrameDrawn = false;
}

/// <summary>
/// シーン切り替え前後のフェード透明度を更新する。
/// </summary>
void Game::UpdateSceneTransition(float dt)
{
    if (impl_->sceneFadePhase == SceneFadePhase::Prepare && impl_->blackFrameDrawn) {
        // 次のシーンを黒で隠して一度描画してから明転を始める。
        impl_->sceneFadePhase = SceneFadePhase::FadeIn;
        impl_->sceneFadeTime = 0.0f;
        impl_->blackFrameDrawn = false;
        return;
    }
    if (impl_->sceneFadePhase != SceneFadePhase::FadeOut && impl_->sceneFadePhase != SceneFadePhase::FadeIn) {
        return;
    }
    impl_->sceneFadeTime += (std::max)(dt, 0.0f);
    const float duration = impl_->sceneFadePhase == SceneFadePhase::FadeOut ? kSceneFadeOutDuration : kSceneFadeInDuration; // 現在段階の演出時間
    const float rate = (std::clamp)(impl_->sceneFadeTime / duration, 0.0f, 1.0f); // フェード段階内の時間率
    const float fadeRate = impl_->sceneFadePhase == SceneFadePhase::FadeOut
        ? (std::clamp)((rate - kSceneFadeOutDarkenStart) / (1.0f - kSceneFadeOutDarkenStart), 0.0f, 1.0f)
        : rate; // 暗転だけ開始を遅らせて収束と白黒化を見せる時間率
    const float easedRate = fadeRate * fadeRate * (3.0f - 2.0f * fadeRate); // 開始と終了を緩やかにする透明度変化率
    impl_->sceneFadeAlpha = impl_->sceneFadePhase == SceneFadePhase::FadeOut ? easedRate : 1.0f - easedRate;
    if (rate >= 1.0f) {
        impl_->sceneFadePhase = impl_->sceneFadePhase == SceneFadePhase::FadeOut ? SceneFadePhase::Black : SceneFadePhase::Idle;
    }
}

/// <summary>
/// 遷移中の最終画面を専用描画先へ集める。
/// </summary>
void Game::BeginSceneTransition(int outputTargetHandle)
{
    if (impl_->sceneFadePhase == SceneFadePhase::Idle || impl_->sceneTransitionCaptureActive ||
        !impl_->sceneTransitionTarget.IsValid() || !impl_->sceneTransitionTarget.HasColorSrv() ||
        !impl_->sceneTransitionIntermediateTarget.IsValid() || !impl_->sceneTransitionIntermediateTarget.HasColorSrv() ||
        !impl_->sceneTransitionPostProcess.IsReady() || !impl_->sceneTransitionPostProcess.IsDistortionReady() ||
        !impl_->sceneTransitionPostProcess.IsRadialBlurReady()) {
        return;
    }
    impl_->sceneTransitionOutputHandle = outputTargetHandle;
    impl_->sceneTransitionTarget.Begin(true);
    impl_->sceneTransitionCaptureActive = true;
}

/// <summary>
/// 現在の描画先へ画像なしの黒いフェードを重ねる。
/// </summary>
void Game::DrawSceneTransition()
{
    if (impl_->sceneTransitionCaptureActive) {
        impl_->sceneTransitionTarget.End();
        const bool isFadeOut = impl_->sceneFadePhase == SceneFadePhase::FadeOut; // 中央へ収束させる段階か
        const float outRate = (std::clamp)(impl_->sceneFadeTime / kSceneFadeOutDuration, 0.0f, 1.0f); // 収束段階の時間率
        const float effectWeight = isFadeOut ? outRate * outRate : impl_->sceneFadeAlpha; // 収束を加速し、明転では滑らかに弱める強度
        impl_->sceneTransitionPostProcess.SetDistortionStrength(kSceneTransitionDistortionStrength * effectWeight);
        impl_->sceneTransitionPostProcess.SetDistortionProgress(
            impl_->sceneFadePhase == SceneFadePhase::FadeOut ? effectWeight : 2.0f - effectWeight);
        impl_->sceneTransitionPostProcess.SetRadialBlurWidth(kSceneTransitionBlurWidth * effectWeight);
        impl_->sceneTransitionPostProcess.SetRadialBlurScale(1.0f -
            (isFadeOut ? kSceneTransitionContraction : kSceneTransitionExpansion) * effectWeight);
        impl_->sceneTransitionPostProcess.SetRadialBlurGrayscaleAmount(
            isFadeOut ? (std::min)(outRate * 1.5f, 1.0f) : effectWeight);
        impl_->sceneTransitionIntermediateTarget.Begin(true);
        impl_->sceneTransitionPostProcess.DrawTexture(impl_->sceneTransitionTarget.GetColorSrvIndex(), PostEffectType::Distortion);
        impl_->sceneTransitionIntermediateTarget.End();
        // Endはバックバッファへ戻すため、Scene Viewの描画先を明示的に復帰する。
        if (impl_->sceneTransitionOutputHandle >= 0) {
            DirectXCommon::GetInstance()->BeginRenderTo(impl_->sceneTransitionOutputHandle, false);
        }
        impl_->sceneTransitionPostProcess.DrawTexture(impl_->sceneTransitionIntermediateTarget.GetColorSrvIndex(), PostEffectType::RadialBlur);
        impl_->sceneTransitionCaptureActive = false;
    }
    if (!impl_->sceneFadeSprite || impl_->sceneFadeAlpha <= 0.0f) {
        return;
    }
    DirectXCommon* directXCommon = DirectXCommon::GetInstance(); // 現在の描画サイズを取得する基盤
    const BlendMode previousBlend = impl_->spriteCommon->GetBlendMode(); // フェード前のスプライト描画設定
    impl_->spriteCommon->SetBlendMode(BlendMode::Alpha);
    impl_->spriteCommon->SetCommonDrawSetting();
    impl_->sceneFadeSprite->SetSize({ static_cast<float>(directXCommon->GetRenderWidth()), static_cast<float>(directXCommon->GetRenderHeight()) });
    impl_->sceneFadeSprite->SetColor({ 0.0f, 0.0f, 0.0f, impl_->sceneFadeAlpha });
    impl_->sceneFadeSprite->Update();
    impl_->sceneFadeSprite->Draw();
    impl_->spriteCommon->SetBlendMode(previousBlend);
    if (impl_->sceneFadePhase == SceneFadePhase::Black || impl_->sceneFadePhase == SceneFadePhase::Prepare) {
        impl_->blackFrameDrawn = true;
    }
}

/// <summary>
/// ゲームの描画処理を行う。
/// </summary>
void Game::Draw()
{

    if (!impl_ || !impl_->initialized) {
        return;
    }

    impl_->imguiManager.NewFrame();

    if (impl_->sceneFadePhase == SceneFadePhase::Black && impl_->blackFrameDrawn) {
        if (impl_->sceneManager && impl_->sceneManager->GetCurrentSceneName() == impl_->pendingSceneName) {
            impl_->pendingSceneName.clear();
        } else {
            auto newScene = GameApp::SceneFactory::Create(impl_->pendingSceneName, impl_->stageSelection);
            if (newScene && impl_->sceneManager) {
                impl_->sceneManager->ChangeScene(std::move(newScene));
            }
            impl_->pendingSceneName.clear();
        }
        impl_->sceneFadePhase = SceneFadePhase::Prepare;
        impl_->sceneFadeTime = 0.0f;
        impl_->blackFrameDrawn = false;
        if (impl_->sceneManager) {
            // 入力を止めた状態で描画行列を整え、このフレームを黒で隠して描画する。
            impl_->sceneManager->Update(0.0f);
        }
    }
    ImGuiManager::Context ctx;
    Matrix4x4 sceneViewMatrix = MathUtil::MakeIdentity4x4();
    Matrix4x4 sceneProjectionMatrix = MathUtil::MakeIdentity4x4();
    const bool useDebugSceneViewCamera = impl_->object3dCommon ? impl_->object3dCommon->GetUseDebugCameraForRender() : impl_->useDebugCameraForRender;
    if (useDebugSceneViewCamera) {
        impl_->debugCamera.Update();
        sceneViewMatrix = impl_->debugCamera.GetViewMatrix();
        sceneProjectionMatrix = impl_->debugCamera.GetProjectionMatrix();
    } else if (impl_->camera) {
        impl_->camera->Update();
        sceneViewMatrix = impl_->camera->GetViewMatrix();
        sceneProjectionMatrix = impl_->camera->GetProjectionMatrix();
    }
    ctx.sceneViewMatrix = &sceneViewMatrix;
    ctx.sceneProjectionMatrix = &sceneProjectionMatrix;
    ctx.particleEmitter = &impl_->pmEmitter;
    ctx.object3dCommon = impl_->object3dCommon.get();
    std::vector<Object3d*> objPtrs;
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillObject3dPointers(&objPtrs);
    } else {
        objPtrs.reserve(impl_->objects3d.size());
        for (auto& o : impl_->objects3d) {
            objPtrs.push_back(o.get());
        }
    }
    ctx.objects3d = &objPtrs;
    ctx.notifyObjectTransformEdited = [this](size_t objectIndex) {
        if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
            impl_->sceneManager->GetCurrent()->NotifyObjectTransformEdited(objectIndex);
        }
    };
    ctx.drawSceneViewOverlay = [this](const Matrix4x4& viewProjectionMatrix, float imageMinX, float imageMinY, float imageWidth, float imageHeight) {
        if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
            impl_->sceneManager->GetCurrent()->DrawSceneViewOverlay(viewProjectionMatrix, imageMinX, imageMinY, imageWidth, imageHeight);
        }
    };
    std::vector<Sprite*> spritePtrs;
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillSpritePointers(&spritePtrs);
    } else {
        spritePtrs.reserve(impl_->sprites.size());
        for (auto& s : impl_->sprites) {
            spritePtrs.push_back(s.get());
        }
    }
    ctx.sprites = &spritePtrs;
    std::vector<ParticleEmitter*> emitterPtrs; // Scene Viewギズモで操作するエミッター一覧
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->FillParticleEmitterPointers(&emitterPtrs);
    }
    if (emitterPtrs.empty()) {
        emitterPtrs.push_back(&impl_->pmEmitter);
    }
    ctx.particleEmitters = &emitterPtrs;
    ctx.spriteCommon = impl_->spriteCommon.get();
    ctx.particleManager = ParticleManager::GetInstance();
    ctx.requestSceneChange = [this](const char* sceneName) {
        if (sceneName) {
            RequestSceneChange(sceneName);
        }
    };
    if (impl_->sceneManager) {
        static std::string sname;
        sname = impl_->sceneManager->GetCurrentSceneName();
        ctx.currentSceneName = sname.c_str();
        if (impl_->sceneManager->GetCurrent()) {
            ctx.postProcess = impl_->sceneManager->GetCurrent()->GetPostProcess();
            ctx.srvManager = &impl_->srvManager;
            ctx.sceneViewSrvIndex = impl_->sceneManager->GetCurrent()->GetSceneViewSrvIndex();
            ctx.sceneViewWidth = DirectXCommon::GetInstance()->GetRenderWidth();
            ctx.sceneViewHeight = DirectXCommon::GetInstance()->GetRenderHeight();
        }
    }
    impl_->imguiManager.BuildUI(ctx);
    if (impl_->sceneManager && impl_->sceneManager->GetCurrent()) {
        impl_->sceneManager->GetCurrent()->DrawImGui();
    }

    DirectXCommon::GetInstance()->PreDraw();
    impl_->srvManager.PreDraw();

    if (impl_->sceneManager) {
        if (impl_->sceneManager->GetCurrent()) {
#ifdef USE_IMGUI
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(true);
#else
            impl_->sceneManager->GetCurrent()->SetSceneViewOnly(false);
#endif
        }
        if (impl_->debugRenderer) {
            impl_->debugRenderer->BeginFrame();
        }
        impl_->sceneManager->Draw();
    }

    impl_->imguiManager.Render(DirectXCommon::GetInstance()->GetCommandList());
    DirectXCommon::GetInstance()->PostDraw();
}

/// <summary>
/// ゲームの終了処理を行う。
/// </summary>
void Game::Finalize()
{
    if (!impl_) {
        return;
    }

    if (!impl_->initialized) {
        return;
    }

    impl_->initialized = false;
    impl_->endRequested = true;

    if (impl_->sceneManager) {
        impl_->sceneManager->Finalize();
        impl_->sceneManager.reset();
    }

    impl_->particlePlane.reset();
    impl_->objects3d.clear();
    impl_->sprites.clear();
    impl_->sceneFadeSprite.reset();
    impl_->sceneTransitionTarget.Finalize();
    impl_->sceneTransitionIntermediateTarget.Finalize();
    impl_->sceneTransitionPostProcess.Finalize();
    impl_->camera.reset();

    ParticleManager::GetInstance()->Finalize();

    impl_->soundData1.reset();
    impl_->soundSystem.Finalize();

    impl_->imguiManager.Shutdown();

    impl_->debugRenderer.reset();

    impl_->spriteCommon.reset();

    // ModelのGPUリソースを解放予約できるよう、DirectXCommon破棄より先にモデルキャッシュを破棄する
    ModelManager::GetInstance()->Finalize();

    if (impl_->object3dCommon) {
        impl_->object3dCommon->Finalize();
        impl_->object3dCommon.reset();
    }

    TextureManager::GetInstance()->Finalize();

    DirectXCommon::GetInstance()->DestroyAllRenderTargets();

    impl_->srvManager.Finalize();

    DirectXCommon::GetInstance()->Finalize();


    InputManager::GetInstance()->Finalize();

    impl_->directInput.Reset();

    impl_->winApp.Finalize();

    Framework::Finalize();
}
/// <summary>
/// 終了要求が出ているかを返す。
/// </summary>
/// <returns>終了要求がある場合は true。</returns>
bool Game::IsEndRequest() const
{
    return impl_ ? impl_->endRequested : true;
}

/// <summary>
/// ウィンドウメッセージを処理し、継続可否を返す。
/// </summary>
/// <returns>処理を継続できる場合は true。</returns>
bool Game::PollEvents()
{
    if (!impl_) {
        return false;
    }

    return impl_->winApp.ProcessMessage();
}
