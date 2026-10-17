/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_network.h"
#include "network_controller.h"
#include "network_layout.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/runtime/async_job_runner.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace app_settings_network;

namespace {

enum class ActionId {
    ScanWifi,
    ConnectWifi,
    DisconnectWifi,
    ForgetWifi,
    StartAp,
    StopAp,
};

enum class ToggleId {
    ExtAntenna,
};

class AppNetwork;

struct ActionBinding {
    AppNetwork* app  = nullptr;
    ActionId id      = ActionId::ScanWifi;
    lv_obj_t* button = nullptr;
};

struct NetworkBinding {
    AppNetwork* app = nullptr;
    std::string ssid;
    lv_obj_t* button = nullptr;
};

struct ToggleBinding {
    AppNetwork* app = nullptr;
    ToggleId id     = ToggleId::ExtAntenna;
};

static const char* wifi_state_name(hal::HalBase::WifiConnectionState state)
{
    using State = hal::HalBase::WifiConnectionState;
    switch (state) {
        case State::Scanning:
            return "Scanning";
        case State::Connecting:
            return "Connecting";
        case State::Connected:
            return "Connected";
        case State::Failed:
            return "Failed";
        case State::ApRunning:
            return "AP running";
        case State::Idle:
        default:
            return "Idle";
    }
}

class AppNetwork : public mooncake::AppAbility {
public:
    AppNetwork() : _controller(makeCallbacks())
    {
        setAppInfo().name = "Network";
    }

    void onOpen() override
    {
        _shell = std::make_unique<app_ui::AppShell>("Network", [&]() { close(); });
        _shell->init();
        _networksRendered = false;
        build();
    }

    void onRunning() override
    {
        const uint32_t now = GetHAL()->millis();
        if (now - _lastUpdateMs < 250) {
            return;
        }
        LvglLockGuard lock;
        refresh();
        _lastUpdateMs = now;
    }

    void onClose() override
    {
        _shell.reset();
        _actions.clear();
        _toggles.clear();
        _networkRows.clear();
    }

private:
    NetworkController _controller;
    std::unique_ptr<app_ui::AppShell> _shell;
    std::vector<std::unique_ptr<ActionBinding>> _actions;
    std::vector<std::unique_ptr<ToggleBinding>> _toggles;
    std::vector<std::unique_ptr<NetworkBinding>> _networkRows;
    std::vector<hal::HalBase::WifiNetwork> _displayedNetworks;
    bool _networksRendered = false;
    bool _scanPending      = false;
    bool _scanComplete     = false;
    bool _busy             = false;
    uint32_t _lastUpdateMs = 0;

    lv_obj_t* _primary          = nullptr;
    lv_obj_t* _secondary        = nullptr;
    lv_obj_t* _status           = nullptr;
    lv_obj_t* _detail           = nullptr;
    lv_obj_t* _ssid             = nullptr;
    lv_obj_t* _password         = nullptr;
    lv_obj_t* _extAntennaSwitch = nullptr;
    lv_obj_t* _networkList      = nullptr;
    lv_obj_t* _networkSummary   = nullptr;
    lv_obj_t* _emptyNetworks    = nullptr;
    lv_obj_t* _apStatus         = nullptr;

    lv_obj_t* content() const
    {
        return _shell->content();
    }

    lv_obj_t* bindButton(const char* text, int x, int y, int w, int h, app_ui::ButtonKind kind, ActionId id)
    {
        auto binding    = std::make_unique<ActionBinding>();
        binding->app    = this;
        binding->id     = id;
        auto* button    = app_ui::makeButton(content(), text, x, y, w, h, kind);
        binding->button = button;
        lv_obj_add_event_cb(button, AppNetwork::onAction, LV_EVENT_CLICKED, binding.get());
        _actions.push_back(std::move(binding));
        return button;
    }

    lv_obj_t* bindToggle(lv_obj_t* parent, const char* text, ToggleId id, bool initial)
    {
        const auto layout = makeNetworkLayout();
        const auto& label = layout.antennaLabel;
        app_ui::makeFittedLabel(parent, text, label.x, label.y, label.w, label.h, &lv_font_montserrat_16,
                                app_ui::theme::color(app_ui::theme::text), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        auto binding = std::make_unique<ToggleBinding>();
        binding->app = this;
        binding->id  = id;
        auto* sw     = app_ui::makeSwitch(parent, layout.extAntennaSwitch.x, layout.extAntennaSwitch.y,
                                          layout.extAntennaSwitch.w, layout.extAntennaSwitch.h, initial);
        lv_obj_add_event_cb(sw, AppNetwork::onToggle, LV_EVENT_VALUE_CHANGED, binding.get());
        _toggles.push_back(std::move(binding));
        return sw;
    }

    void build()
    {
        LvglLockGuard lock;
        auto layout = makeNetworkLayout();
        _primary    = app_ui::makeMetricCard(content(), "Wi-Fi", "--", layout.wifiCard.x, layout.wifiCard.y,
                                             layout.wifiCard.w, layout.wifiCard.h);
        _secondary  = app_ui::makeMetricCard(content(), "Address", "--", layout.addressCard.x, layout.addressCard.y,
                                             layout.addressCard.w, layout.addressCard.h);
        _status     = app_ui::makeWrappedLabel(content(), "", layout.statusText.x, layout.statusText.y,
                                               &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_status, layout.statusText.w);
        lv_obj_set_height(_status, layout.statusText.h);

        auto* form = app_ui::makePanel(content(), layout.stationPanel.x, layout.stationPanel.y, layout.stationPanel.w,
                                       layout.stationPanel.h);
        app_ui::makeWrappedLabel(form, "Station Connection", 0, 0, &lv_font_montserrat_20,
                                 app_ui::theme::color(app_ui::theme::text));
        _ssid     = app_ui::makeTextArea(form, "SSID", layout.ssidInput.x, layout.ssidInput.y, layout.ssidInput.w,
                                         layout.ssidInput.h);
        _password = app_ui::makeTextArea(form, "Password", layout.passwordInput.x, layout.passwordInput.y,
                                         layout.passwordInput.w, layout.passwordInput.h, true);
        _shell->bindTextInput(_ssid);
        _shell->bindTextInput(_password);
        auto* networks = app_ui::makePanel(content(), layout.networksPanel.x, layout.networksPanel.y,
                                           layout.networksPanel.w, layout.networksPanel.h);
        app_ui::makeLabel(networks, "Nearby Networks", 0, 4, &lv_font_montserrat_20,
                          app_ui::theme::color(app_ui::theme::text));
        _networkSummary =
            app_ui::makeFittedLabel(networks, "Scan to find networks", 0, 36, 380, 20, &lv_font_montserrat_14,
                                    app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        const auto& list = layout.networksList;
        _networkList     = app_ui::makePanel(networks, list.x, list.y, list.w, list.h);
        lv_obj_set_style_pad_all(_networkList, 0, 0);
        lv_obj_set_style_border_width(_networkList, 0, 0);
        lv_obj_set_style_bg_opa(_networkList, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(_networkList, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(_networkList, LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_remove_flag(_networkList, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_remove_flag(_networkList, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_scroll_dir(_networkList, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(_networkList, LV_SCROLLBAR_MODE_AUTO);
        bindButton("Scan", layout.scanButton.x, layout.scanButton.y, layout.scanButton.w, layout.scanButton.h,
                   app_ui::ButtonKind::Secondary, ActionId::ScanWifi);
        bindButton("Connect", layout.connectButton.x, layout.connectButton.y, layout.connectButton.w,
                   layout.connectButton.h, app_ui::ButtonKind::Primary, ActionId::ConnectWifi);
        bindButton("Disconnect", layout.disconnectButton.x, layout.disconnectButton.y, layout.disconnectButton.w,
                   layout.disconnectButton.h, app_ui::ButtonKind::Secondary, ActionId::DisconnectWifi);
        bindButton("Forget", layout.forgetButton.x, layout.forgetButton.y, layout.forgetButton.w, layout.forgetButton.h,
                   app_ui::ButtonKind::Danger, ActionId::ForgetWifi);

        auto* ap = app_ui::makePanel(content(), layout.apPanel.x, layout.apPanel.y, layout.apPanel.w, layout.apPanel.h);
        app_ui::makeWrappedLabel(ap, "Access Point", 0, 0, &lv_font_montserrat_20,
                                 app_ui::theme::color(app_ui::theme::text));
        _apStatus = app_ui::makeWrappedLabel(ap, "Stopped", layout.apDescription.x, layout.apDescription.y,
                                             &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_apStatus, layout.apDescription.w);
        lv_obj_set_height(_apStatus, layout.apDescription.h);
        _extAntennaSwitch = bindToggle(ap, "External antenna", ToggleId::ExtAntenna, GetHAL()->getExtAntennaEnable());
        bindButton("Start AP", layout.startApButton.x, layout.startApButton.y, layout.startApButton.w,
                   layout.startApButton.h, app_ui::ButtonKind::Primary, ActionId::StartAp);
        bindButton("Stop AP", layout.stopApButton.x, layout.stopApButton.y, layout.stopApButton.w,
                   layout.stopApButton.h, app_ui::ButtonKind::Secondary, ActionId::StopAp);

        _detail = app_ui::makeWrappedLabel(content(), "", layout.detailText.x, layout.detailText.y,
                                           &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(_detail, layout.detailText.w);
        lv_obj_set_height(_detail, layout.detailText.h);
        refresh();
    }

    void refresh()
    {
        auto snapshot = _controller.refresh();
        if (_scanPending && !snapshot.busy) {
            _scanPending  = false;
            _scanComplete = true;
        }
        _busy = snapshot.busy || snapshot.updateActive ||
                snapshot.status.state == hal::HalBase::WifiConnectionState::Connecting ||
                snapshot.status.state == hal::HalBase::WifiConnectionState::Scanning;
        refreshMetric(_primary, "Wi-Fi", "%s", wifi_state_name(snapshot.status.state));
        refreshMetric(_secondary, "Address", "%s",
                      snapshot.status.ipAddress.empty() ? "--" : snapshot.status.ipAddress.c_str());

        if (_status != nullptr) {
            const auto signal = snapshot.status.state == hal::HalBase::WifiConnectionState::Connected
                                    ? std::to_string(snapshot.status.rssi) + " dBm"
                                    : "--";
            lv_label_set_text_fmt(_status, "SSID: %s\nRSSI: %s\nExt antenna: %s",
                                  snapshot.status.ssid.empty() ? "--" : snapshot.status.ssid.c_str(), signal.c_str(),
                                  snapshot.status.extAntennaEnabled ? "on" : "off");
        }
        if (_extAntennaSwitch != nullptr) {
            if (snapshot.status.extAntennaEnabled) {
                lv_obj_add_state(_extAntennaSwitch, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(_extAntennaSwitch, LV_STATE_CHECKED);
            }
        }
        const bool apRunning = snapshot.status.mode == hal::HalBase::WifiMode::AccessPoint ||
                               snapshot.status.mode == hal::HalBase::WifiMode::StationAndAp;
        lv_label_set_text(_apStatus, apRunning ? "Running" : "Stopped");
        for (const auto& action : _actions) {
            bool disabled = _busy;
            if (action->id == ActionId::ConnectWifi) {
                disabled = disabled || apRunning || lv_textarea_get_text(_ssid)[0] == '\0';
            } else if (action->id == ActionId::ScanWifi) {
                disabled = disabled || apRunning;
            } else if (action->id == ActionId::DisconnectWifi) {
                const auto state = snapshot.status.state;
                disabled         = snapshot.busy || snapshot.updateActive ||
                           (state != hal::HalBase::WifiConnectionState::Connected &&
                            state != hal::HalBase::WifiConnectionState::Connecting);
            } else if (action->id == ActionId::StartAp) {
                disabled = disabled || apRunning;
            } else if (action->id == ActionId::StopAp) {
                disabled = disabled || !apRunning;
            }
            if (disabled) {
                lv_obj_add_state(action->button, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(action->button, LV_STATE_DISABLED);
            }
        }
        for (auto* field : {_ssid, _password, _extAntennaSwitch}) {
            if (_busy) {
                lv_obj_add_state(field, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(field, LV_STATE_DISABLED);
            }
        }
        renderNetworks(snapshot);
        showDetail(snapshot);
    }

    template <typename... Args>
    void refreshMetric(lv_obj_t* card, const char* title, const char* fmt, Args... args)
    {
        if (card == nullptr) {
            return;
        }
        char value[96];
        std::snprintf(value, sizeof(value), fmt, args...);
        auto* valueLabel = lv_obj_get_child(card, 1);
        if (valueLabel != nullptr) {
            lv_label_set_text(valueLabel, value);
        }
        auto* titleLabel = lv_obj_get_child(card, 0);
        if (titleLabel != nullptr) {
            lv_label_set_text(titleLabel, title);
        }
    }

    void showDetail(const NetworkSnapshot& snapshot)
    {
        if (_detail == nullptr) {
            return;
        }
        std::string text = snapshot.detail;
        if (snapshot.updateActive) {
            text = "Firmware update owns Wi-Fi. Cancel or finish it in Updates.";
        } else if (snapshot.status.state == hal::HalBase::WifiConnectionState::Connecting) {
            text = "Connecting to " + snapshot.status.ssid + "...";
        } else if (!snapshot.status.lastError.empty()) {
            text = snapshot.status.lastError;
        }
        lv_label_set_text(_detail, text.c_str());
        lv_obj_set_style_text_color(
            _detail,
            app_ui::theme::color(snapshot.status.state == hal::HalBase::WifiConnectionState::Failed
                                     ? app_ui::theme::red
                                     : app_ui::theme::muted),
            0);
    }

    void renderNetworks(const NetworkSnapshot& snapshot)
    {
        const auto layout     = makeNetworkLayout();
        const bool scrollable = networkRowsHeight(layout, snapshot.networks.size()) > layout.networksList.h;
        if (_scanPending)
            lv_label_set_text(_networkSummary, "Scanning...");
        else
            lv_label_set_text_fmt(_networkSummary, "%u networks%s", static_cast<unsigned>(snapshot.networks.size()),
                                  scrollable ? " | Scroll to see all" : "");
        const bool changed = !_networksRendered || snapshot.networks.size() != _displayedNetworks.size() ||
                             !std::equal(snapshot.networks.begin(), snapshot.networks.end(), _displayedNetworks.begin(),
                                         [](const auto& a, const auto& b) {
                                             return a.ssid == b.ssid && a.rssi == b.rssi && a.secure == b.secure &&
                                                    a.channel == b.channel;
                                         });
        if (changed) {
            lv_obj_clean(_networkList);
            lv_obj_scroll_to_y(_networkList, 0, LV_ANIM_OFF);
            _networkRows.clear();
            _emptyNetworks     = nullptr;
            _displayedNetworks = snapshot.networks;
            _networksRendered  = true;
            if (_displayedNetworks.empty()) {
                _emptyNetworks = app_ui::makeFittedLabel(_networkList, "", 16, 16, 520, 60, &lv_font_montserrat_18,
                                                         app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_WRAP,
                                                         LV_TEXT_ALIGN_LEFT);
            }
            size_t index = 0;
            for (const auto& network : _displayedNetworks) {
                auto binding       = std::make_unique<NetworkBinding>();
                binding->app       = this;
                binding->ssid      = network.ssid;
                const auto rowRect = networkRowRect(layout, index++);
                auto* row          = app_ui::makeButtonSurface(_networkList, rowRect.x, rowRect.y, rowRect.w, rowRect.h,
                                                               app_ui::theme::color(app_ui::theme::panelAlt),
                                                               app_ui::theme::color(app_ui::theme::border), 1, 8);
                binding->button    = row;
                lv_obj_set_style_bg_color(row, app_ui::theme::color(app_ui::theme::blue), LV_STATE_CHECKED);
                app_ui::makeFittedLabel(row, network.ssid.empty() ? "Hidden network" : network.ssid.c_str(), 16, 5, 390,
                                        40, &lv_font_montserrat_16, app_ui::theme::color(app_ui::theme::text),
                                        LV_LABEL_LONG_WRAP, LV_TEXT_ALIGN_LEFT);
                char security[64];
                std::snprintf(security, sizeof(security), "%s | Channel %u", network.secure ? "Secured" : "Open",
                              static_cast<unsigned>(network.channel));
                app_ui::makeFittedLabel(row, security, 16, 50, 390, 20, &lv_font_montserrat_14,
                                        app_ui::theme::color(app_ui::theme::muted), LV_LABEL_LONG_DOT,
                                        LV_TEXT_ALIGN_LEFT);
                char signal[24];
                std::snprintf(signal, sizeof(signal), "%d dBm", network.rssi);
                app_ui::makeFittedLabel(row, signal, 420, 26, 118, 24, &lv_font_montserrat_16,
                                        app_ui::theme::color(app_ui::theme::text), LV_LABEL_LONG_DOT,
                                        LV_TEXT_ALIGN_RIGHT);
                lv_obj_add_event_cb(
                    row,
                    [](lv_event_t* event) {
                        auto* binding = static_cast<NetworkBinding*>(lv_event_get_user_data(event));
                        if (!binding->app->_busy && !binding->ssid.empty()) {
                            if (binding->ssid != lv_textarea_get_text(binding->app->_ssid)) {
                                lv_textarea_set_text(binding->app->_ssid, binding->ssid.c_str());
                                lv_textarea_set_text(binding->app->_password, "");
                            }
                            binding->app->refresh();
                        }
                    },
                    LV_EVENT_CLICKED, binding.get());
                _networkRows.push_back(std::move(binding));
            }
        }
        if (_emptyNetworks != nullptr) {
            lv_label_set_text(_emptyNetworks, _scanPending    ? "Scanning..."
                                              : _scanComplete ? "No networks found"
                                                              : "Not scanned");
        }
        for (const auto& row : _networkRows) {
            if (row->ssid == lv_textarea_get_text(_ssid) && !row->ssid.empty()) {
                lv_obj_add_state(row->button, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(row->button, LV_STATE_CHECKED);
            }
            if (_busy || row->ssid.empty()) {
                lv_obj_add_state(row->button, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(row->button, LV_STATE_DISABLED);
            }
        }
    }

    void handle(ActionId id)
    {
        switch (id) {
            case ActionId::ScanWifi:
                if (_controller.scan()) {
                    _scanPending = true;
                }
                break;
            case ActionId::ConnectWifi:
                _controller.connect(lv_textarea_get_text(_ssid), lv_textarea_get_text(_password));
                break;
            case ActionId::DisconnectWifi:
                _controller.disconnect();
                break;
            case ActionId::ForgetWifi:
                _controller.forgetCredentials();
                break;
            case ActionId::StartAp:
                _controller.startAccessPoint();
                break;
            case ActionId::StopAp:
                _controller.stopAccessPoint();
                break;
        }
        refresh();
    }

    void handleToggle(ToggleId id, bool checked)
    {
        switch (id) {
            case ToggleId::ExtAntenna:
                _controller.setExtAntennaEnabled(checked);
                break;
        }
        refresh();
    }

    static NetworkCallbacks makeCallbacks()
    {
        NetworkCallbacks callbacks;
        callbacks.status    = [] { return GetHAL()->wifiGetStatus(); };
        callbacks.scan      = [] { return GetHAL()->wifiScan(); };
        callbacks.otaStatus = [] { return GetHAL()->otaGetStatus(); };
        callbacks.connect   = [](const std::string& ssid, const std::string& password) {
            return GetHAL()->wifiConnect(ssid, password);
        };
        callbacks.disconnect        = [] { GetHAL()->wifiDisconnect(); };
        callbacks.forgetCredentials = [] { GetHAL()->wifiForgetCredentials(); };
        callbacks.setExtAntenna     = [](bool enabled) { GetHAL()->wifiSetExtAntennaEnabled(enabled); };
        callbacks.startAp           = [] { return GetHAL()->wifiStartAccessPoint(); };
        callbacks.stopAp            = [] { GetHAL()->wifiStopAccessPoint(); };
        callbacks.startJob          = [](const std::string& name, std::function<void()> work) {
            return app_runtime::globalAsyncJobRunner().start(name.c_str(), std::move(work));
        };
        return callbacks;
    }

    static void onAction(lv_event_t* event)
    {
        auto* binding = static_cast<ActionBinding*>(lv_event_get_user_data(event));
        if (binding != nullptr && binding->app != nullptr) {
            binding->app->handle(binding->id);
        }
    }

    static void onToggle(lv_event_t* event)
    {
        auto* binding = static_cast<ToggleBinding*>(lv_event_get_user_data(event));
        if (binding != nullptr && binding->app != nullptr) {
            binding->app->handleToggle(
                binding->id, lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(event)), LV_STATE_CHECKED));
        }
    }
};

}  // namespace

std::unique_ptr<mooncake::AppAbility> app_settings_network::makeNetworkApp()
{
    return std::make_unique<AppNetwork>();
}
