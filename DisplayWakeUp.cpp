// DisplayWakeUp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <numeric>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Display.Core.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Devices::Display;
using namespace Windows::Devices::Display::Core;


#define CATCH_AND_LOG(program_code, error)         \
try {                                              \
    program_code                                   \
}                                                  \
catch (winrt::hresult_error const& ex) {           \
    winrt::hresult hr = ex.code();                 \
    winrt::hstring message = ex.message();         \
    wprintf(error" Code: %ls\n", message.c_str()); \
    throw;                                         \
}                           

void RationalReduce(Rational& rat)
{
    int gcd = std::gcd(rat.Numerator, rat.Denominator);
    int num = rat.Numerator / gcd;
    int den = rat.Denominator / gcd;

    if (den < 0) {
        den = -den;
        num = -num;
    }

    rat.Numerator = (uint32_t)num;
    rat.Denominator = (uint32_t)den;
}

bool RationalLessThan(Rational lhs, Rational rhs)
{
    RationalReduce(lhs);
    RationalReduce(rhs);

    return lhs.Numerator* rhs.Denominator < rhs.Numerator* lhs.Denominator;
}

DisplayModeInfo FindLowestPixelCountAndHzMode(const IVectorView<DisplayModeInfo> modes)
{
    std::vector<std::pair<DisplayModeInfo, size_t>> pixelCountsPerMode;
    pixelCountsPerMode.reserve(modes.Size());

    for (auto&& mode : modes)
    {
        const size_t pixelCount = mode.TargetResolution().Height * mode.TargetResolution().Width;
        pixelCountsPerMode.push_back({ mode, pixelCount });   
    }

    std::pair<DisplayModeInfo, size_t> min = *std::min_element(pixelCountsPerMode.begin(), pixelCountsPerMode.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.second < rhs.second;
    });

    auto end_range = std::remove_if(pixelCountsPerMode.begin(), pixelCountsPerMode.end(), [&min](std::pair<DisplayModeInfo, size_t> val) {
        return (val.second > min.second); }
    );

    std::sort(pixelCountsPerMode.begin(), end_range, [&](std::pair<DisplayModeInfo, size_t>& lhs, std::pair<DisplayModeInfo, size_t>& rhs) {
        auto lhs_rate = lhs.first.PresentationRate().VerticalSyncRate;
        auto rhs_rate = rhs.first.PresentationRate().VerticalSyncRate;

        return RationalLessThan(lhs_rate, rhs_rate);
    });

    return pixelCountsPerMode[0].first;
}

void WakeDisplayWithDefaultMode(const DisplayTarget displayTarget, const DisplayManager manager, const bool bUsePreferredRes)
{
    DisplayDevice dev = nullptr;
    CATCH_AND_LOG(
        dev = manager.CreateDisplayDevice(displayTarget.Adapter()); , L"Failed to create display dev"
    )

    DisplayManagerResultWithState stateRes = nullptr; 
    
    CATCH_AND_LOG(
        stateRes = manager.TryAcquireTargetsAndCreateEmptyState({ displayTarget });, L"Failed to create empty state"
    )

    const DisplayState state = stateRes.State();
    const DisplayPath path = state.ConnectTarget(displayTarget);

    const IVectorView<DisplayModeInfo> modes = path.FindModes(bUsePreferredRes ? DisplayModeQueryOptions::OnlyPreferredResolution : DisplayModeQueryOptions::None);
    if (!modes.Size())
    {
        printf(__FUNCTION__" Failed to find any modes for display\n");
        return;
    }
    
    const DisplayModeInfo targetMode = FindLowestPixelCountAndHzMode(modes);

    CATCH_AND_LOG(
        path.ApplyPropertiesFromMode(targetMode);, L"Failed to apply properties from display mode"
    )

    CATCH_AND_LOG(
        state.TryApply(DisplayStateApplyOptions::None);, L"Failed to apply display state"
    )
}

void CreateConsole()
{
    FILE* stream;

    AllocConsole();
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    const int argc = __argc;
    WCHAR** const argv = __wargv;

    bool bUsePreferredRes = true;
    bool bShowConsole = true;

    if (argc > 1)
    {
        for (size_t i = 1; i < argc; i++)
        {
            const WCHAR* const pArg = argv[i];

            if (lstrcmpiW(pArg, L"--min") == 0)
                bUsePreferredRes = false;
            else if (lstrcmpiW(pArg, L"--no-console") == 0)
                bShowConsole = false;
        }
    }

    if (bShowConsole)
        CreateConsole();

    winrt::init_apartment();

    const DisplayManager manager = DisplayManager::Create(DisplayManagerOptions::None);
    const IVectorView<DisplayTarget> targets = manager.GetCurrentTargets();

    for (auto&& target : targets)
    {
        if (target.UsageKind() == DisplayMonitorUsageKind::SpecialPurpose)
        {
            try {
                WakeDisplayWithDefaultMode(target, manager, bUsePreferredRes);
            }
            catch (...) {
                printf("Failed to wake display\n");
            }
        }
    }

    Sleep(INFINITE);
}
