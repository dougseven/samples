﻿//
// Copyright (c) 2015, Microsoft Corporation
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
// IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "Thread.h"

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::System::Threading;
using namespace MockAdapter;

// The Blank Application template is documented at http://go.microsoft.com/fwlink/?LinkId=234227

/// <summary>
/// Initializes the singleton application object. This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
}

/// <summary>
/// Invoked when the application is launched normally by the end user. Other entry points
/// will be used when the application is launched to open a specific file, to display
/// search results, and so forth.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(LaunchActivatedEventArgs^ e)
{
#if _DEBUG
    if (IsDebuggerPresent())
    {
        //DebugSettings->EnableFrameRateCounter = true;
    }
#endif

    auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

    // Do not repeat app initialization when the Window already has content,
    // just ensure that the window is active.
    if (rootFrame == nullptr)
    {
        // Create a Frame to act as the navigation context and associate it with
        // a SuspensionManager key
        rootFrame = ref new Frame();

        // Change this value to a cache size that is appropriate for your application.
        rootFrame->CacheSize = 1;

        if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
        {
            // Restore the saved session state only when appropriate, scheduling the
            // final launch steps after the restore is complete.
        }

        // Place the frame in the current Window
        Window::Current->Content = rootFrame;
        rootFrame->Navigate(TypeName(MainPage::typeid));

        auto dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

        ThreadPool::RunAsync(ref new WorkItemHandler([&, dispatcher](IAsyncAction^ operation)
        {
            try
            {
                auto mockadapter = ref new AdapterLib::MockAdapter();
                dsbBridge = ref new BridgeRT::DsbBridge(mockadapter);

                if (this->dsbBridge == nullptr)
                {
                    throw ref new Exception(ERROR_NOT_READY, "DSB Bridge not instantiated!");
                }

                HRESULT hr = this->dsbBridge->Initialize();
                if (FAILED(hr))
                {
                    throw  ref new Exception(hr, "DSB Bridge initialization failed!");
                }
            }
            catch (Exception ^ ex)
            {
                dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
                    ref new Windows::UI::Core::DispatchedHandler([this, ex]()
                {
                    BridgeViewModel->InitializationErrorMessage = ex->ToString();
                }));
            }
        }));
    }

    if (rootFrame->Content == nullptr)
    {
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        // Removes the turnstile navigation for startup.
        if (rootFrame->ContentTransitions != nullptr)
        {
            _transitions = ref new TransitionCollection();
            for (auto transition : rootFrame->ContentTransitions)
            {
                _transitions->Append(transition);
            }
        }

        rootFrame->ContentTransitions = nullptr;
        _firstNavigatedToken = rootFrame->Navigated += ref new NavigatedEventHandler(this, &App::RootFrame_FirstNavigated);
#endif

        // When the navigation stack isn't restored navigate to the first page,
        // configuring the new page by passing required information as a navigation
        // parameter.
        if (!rootFrame->Navigate(MainPage::typeid, e->Arguments))
        {
            throw ref new FailureException("Failed to create initial page");
        }
    }


    // Ensure the current window is active
    Window::Current->Activate();
}

#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
/// <summary>
/// Restores the content transitions after the app has launched.
/// </summary>
void App::RootFrame_FirstNavigated(Object^ sender, NavigationEventArgs^ e)
{
    auto rootFrame = safe_cast<Frame^>(sender);

    TransitionCollection^ newTransitions;
    if (_transitions == nullptr)
    {
        newTransitions = ref new TransitionCollection();
        newTransitions->Append(ref new NavigationThemeTransition());
    }
    else
    {
        newTransitions = _transitions;
    }

    rootFrame->ContentTransitions = newTransitions;

    rootFrame->Navigated -= _firstNavigatedToken;
}
#endif

/// <summary>
/// Invoked when application execution is being suspended. Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
    (void)sender;	// Unused parameter
    (void)e;		// Unused parameter

    if (this->dsbBridge != nullptr)
    {
        this->dsbBridge->Shutdown();
    }
}

DsbCommon::DsbBridgeViewModel^ App::BridgeViewModel = ref new DsbCommon::DsbBridgeViewModel();

