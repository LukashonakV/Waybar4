#include "client.hpp"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "supply/clara.hpp"

#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gtkmm/icontheme.h>
#include <glibmm/main.h>
#include <gdkmm/monitor.h>
#include <gtkmm/settings.h>
#include <iostream>
#include <spdlog/spdlog.h>

waybar::Client* waybar::Client::inst() {
  static auto* c{new Client()};
  return c;
}

int waybar::Client::main(int argc, char* argv[]) {
  bool show_help{false};
  bool show_version{false};
  std::string config_opt{};
  std::string style_opt{};
  std::string log_level{};
  auto cli = clara::detail::Help(show_help) |
             clara::detail::Opt(show_version)["-v"]["--version"]("Show version") |
             clara::detail::Opt(config_opt, "config")["-c"]["--config"]("Config path") |
             clara::detail::Opt(style_opt, "style")["-s"]["--style"]("Style path") |
             clara::detail::Opt(
                 log_level,
                 "trace|debug|info|warning|error|critical|off")["-l"]["--log-level"]("Log level") |
             clara::detail::Opt(m_bar_id_, "id")["-b"]["--bar"]("Bar id");
  auto res = cli.parse(clara::detail::Args(argc, argv));
  if (!res) {
    spdlog::error("Error in command line: {}", res.errorMessage());
    return 1;
  }
  if (show_help) {
    std::cout << cli << '\n';
    return 0;
  }
  if (show_version) {
    std::cout << "Waybar v" << VERSION << '\n';
    return 0;
  }
  if (!log_level.empty()) {
    spdlog::set_level(spdlog::level::from_str(log_level));
  }
  m_gtk_app_ = Gtk::Application::create("fr.arouillard.waybar",
                                        Gio::Application::Flags::HANDLES_COMMAND_LINE);
  if (!m_portal_) {
    m_portal_ = std::make_unique<waybar::Portal>();
  }

  m_gdk_display_ = Gdk::Display::get_default(); 
  m_wl_display_ = gdk_wayland_display_get_wl_display(m_gdk_display_->gobj());

  // Initialize Waybars GTK resources with our custom icons
  Gtk::IconTheme::get_for_display(m_gdk_display_)->add_resource_path("/fr/arouillard/waybar/icons");
  // Setup CSS
  m_cssFile_ = getStyle(style_opt);
  setupCss(m_cssFile_);
  m_css_reload_helper_ = std::make_unique<CssReloadHelper>(m_cssFile_, [&]() { setupCss(m_cssFile_); });

  m_portal_->signal_appearance_changed().connect([&](waybar::Appearance appearance) {
    auto css_file = getStyle(style_opt, appearance);
    setupCss(css_file);
  });
  // Get config
  m_config_.load(config_opt);
  const auto cfg{m_config_.getConfig()};
  if (cfg.isObject() && cfg["reload_style_on_change"].asBool()) {
    m_css_reload_helper_->monitorChanges();
  } else if (cfg.isArray()) {
    for (const auto& conf : cfg) {
      if (conf["reload_style_on_change"].asBool()) {
        m_css_reload_helper_->monitorChanges();
        break;
      }
    }
  }

  // Setup bindings
  bindInterfaces();
  
  m_gtk_app_->hold();
  m_gtk_app_->run();
  m_css_reload_helper_.reset();
  m_portal_.reset();

  return 0;
}

const std::string waybar::Client::getStyle(const std::string& style,
                                           std::optional<Appearance> appearance) {
  const auto gtk_settings{Gtk::Settings::get_default()};
  std::optional<std::string> css_file{};
  if (style.empty()) {
    std::vector<std::string> search_files{};
    switch (appearance.value_or(m_portal_->getAppearance())) {
      case waybar::Appearance::LIGHT:
        search_files.emplace_back("style-light.css");
        gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        break;
      case waybar::Appearance::DARK:
        search_files.emplace_back("style-dark.css");
        gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        break;
      case waybar::Appearance::UNKNOWN:
        break;
    }
    search_files.emplace_back("style.css");
    css_file = Config::findConfigPath(search_files);
  } else {
    css_file = style;
  }

  if (!css_file) {
    throw std::runtime_error("Missing required resource files");
  }

  spdlog::info("Using CSS file {}", css_file.value());
  return css_file.value();
};

auto waybar::Client::setupCss(const std::string& css_file) -> void {
  if (m_css_provider_) {
    Gtk::StyleContext::remove_provider_for_display(m_gdk_display_, m_css_provider_);
    m_css_provider_.reset();
  } else
    m_css_provider_ = Gtk::CssProvider::create();

  try {
    m_css_provider_->load_from_path(css_file);
  } catch (const Glib::Error &e) {
    m_css_provider_.reset();
    spdlog::error("{}", e.what());
  }

  Gtk::StyleContext::add_provider_for_display(m_gdk_display_, m_css_provider_,
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

auto waybar::Client::bindInterfaces() -> void {
  m_registry_ = wl_display_get_registry(m_wl_display_);
  static const struct wl_registry_listener registry_listener = {
      .global = onWlGlobal,
      .global_remove = onWlGlobalRemove,
  };
  wl_registry_add_listener(m_registry_, &registry_listener, this);
  wl_display_roundtrip(m_wl_display_);

  if (gtk_layer_is_supported() == 0) {
    throw std::runtime_error("The Wayland compositor does not support wlr-layer-shell protocol");
  }

  if (m_xdg_output_manager_ == nullptr) {
    throw std::runtime_error("Failed to acquire required resources.");
  }

  // Handle subscriptions
  m_monitors_ = m_gdk_display_->get_monitors();

  for (guint i{0}; i < m_monitors_->get_n_items(); ++i) {
    onMonAdd(m_monitors_->get_typed_object<Gdk::Monitor>(i));
  }

  m_monitors_->signal_items_changed().connect([=, this](const guint &position, const guint &removed,
                                                        const guint &added) {
    for (auto i{removed}; i >= 0; --i) {
      onMonRemove(m_monitors_->get_typed_object<Gdk::Monitor>(position + i));
    }

    for (auto i{added}; i >= 0; --i) {
      onMonAdd(m_monitors_->get_typed_object<Gdk::Monitor>(position + i));
    }
    
  });
}

auto waybar::Client::onMonAdd(const Glib::RefPtr<Gdk::Monitor> monitor) -> void {
  spdlog::debug("waybar::Client::onMonAdd\nModel:\t\t{}\nManufacturer:\t{}\nConnector:\t{}\nDescription:\t{}",
                monitor->get_model().c_str(), monitor->get_manufacturer().c_str(),
                monitor->get_connector().c_str(), monitor->get_description().c_str());
  static const struct zxdg_output_v1_listener xdgOutputListener = {
    .logical_position = [](void*, struct zxdg_output_v1*, int32_t, int32_t) {},
    .logical_size = [](void*, struct zxdg_output_v1*, int32_t, int32_t) {},
    .done = &onOutputDone,
    .name = [](void*, struct zxdg_output_v1*, const char*) {},
    .description = [](void*, struct zxdg_output_v1*, const char*) {},
  };
  // owned by output->monitor; no need to destroy
  auto* wl_output{gdk_wayland_monitor_get_wl_output(monitor->gobj())};
  auto* xdg_output{zxdg_output_manager_v1_get_xdg_output(m_xdg_output_manager_, wl_output)};
  zxdg_output_v1_add_listener(xdg_output, &xdgOutputListener, monitor.get());
}

auto waybar::Client::onMonRemove(const Glib::RefPtr<Gdk::Monitor> monitor) -> void {
  spdlog::debug("Output removing is done: {} ({})", monitor->get_connector().c_str(),
                monitor->get_description().c_str());
  /* This event can be triggered from wl_display_roundtrip called by GTK or our code.
   * Defer destruction of bars for the output to the next iteration of the event loop to avoid
   * deleting objects referenced by currently executed code.
   */
  Glib::signal_idle().connect_once(
      sigc::bind(sigc::mem_fun(*this, &Client::onMonRemoveDeffered), monitor),
      Glib::PRIORITY_HIGH_IDLE); 
}

auto waybar::Client::onMonRemoveDeffered(const Glib::RefPtr<Gdk::Monitor> monitor) -> void {
//   for (auto it = bars.begin(); it != bars.end();) {
//    if ((*it)->output->monitor == monitor) {
//      auto output_name = (*it)->output->name;
//      (*it)->window.hide();
//      gtk_app->remove_window((*it)->window);
//      it = bars.erase(it);
//      spdlog::info("Bar removed from output: {}", output_name);
//    } else {
//      ++it;
//    }
//  }
}


auto waybar::Client::onWlGlobalRemove(void* data, struct wl_registry* /*registry*/,
                                      uint32_t name) -> void {
  // Nothing here
}

auto waybar::Client::onWlGlobal(void* data, struct wl_registry* registry, uint32_t name,
                                const char* interface, uint32_t version) -> void {
  auto* client = static_cast<Client*>(data);

  if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
      version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
    client->setXdgOutputManager(static_cast<xdg_output_manager_ptr>(
                        wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface,
                                         ZXDG_OUTPUT_V1_NAME_SINCE_VERSION)));
  } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
    client->setIdleInhibitManager(static_cast<idle_inhibit_manager_ptr>(
        wl_registry_bind(registry, name, &zwp_idle_inhibit_manager_v1_interface, 1)));
  }
 
}

auto waybar::Client::setXdgOutputManager(const xdg_output_manager_ptr val) -> void {
  if (m_xdg_output_manager_ != nullptr)
    zxdg_output_manager_v1_destroy(m_xdg_output_manager_);
  // zxdg_output_manager_v1_destroy must be called first
  m_xdg_output_manager_ = val;
}

auto waybar::Client::setIdleInhibitManager(const idle_inhibit_manager_ptr val) -> void {
  if (m_idle_inhibit_manager_ != nullptr)
    zwp_idle_inhibit_manager_v1_destroy(m_idle_inhibit_manager_);
  // zwp_idle_inhibit_manager_v1_destroy must be called first
  m_idle_inhibit_manager_ = val;
}

auto waybar::Client::onOutputDone(void* user_data, struct zxdg_output_v1*) -> void {
  try {
    auto monitor = static_cast<Glib::RefPtr<Gdk::Monitor>>((Gdk::Monitor*)user_data);
    spdlog::debug("Output detection is done: {} ({})", monitor->get_connector().c_str(),
                  monitor->get_description().c_str());
    auto client{Client::inst()};
    auto configs{client->getOutputCfg(monitor)};
    if (!configs.empty()) {
      for (const auto& config : configs) {
//        client->m_bars_.emplace_back(std::make_unique<Bar>());
      }
    }
  } catch (const std::exception& e) {
    spdlog::warn("caught exception in zxdg_output_v1_listener::done: {}", e.what());
  }
}

auto waybar::Client::getOutputCfg(const Glib::RefPtr<Gdk::Monitor>& monitor) -> std::vector<Json::Value> {
  return m_config_.getOutputConfigs(monitor->get_connector(), monitor->get_description());
}

auto waybar::Client::reset() -> void {
  m_gtk_app_->quit();
  // delete signal handler
  m_portal_->signal_appearance_changed().clear();
}
/*
struct waybar::waybar_output& waybar::Client::getOutput(void* addr) {
  auto it = std::find_if(outputs_.begin(), outputs_.end(),
                         [&addr](const auto& output) { return &output == addr; });
  if (it == outputs_.end()) {
    throw std::runtime_error("Unable to find valid output");
  }
  return *it;
}
int waybar::Client::main(int argc, char* argv[]) {
  bool show_help = false;
  bool show_version = false;
  std::string config_opt;
  std::string style_opt;
  std::string log_level;
  auto cli = clara::detail::Help(show_help) |
             clara::detail::Opt(show_version)["-v"]["--version"]("Show version") |
             clara::detail::Opt(config_opt, "config")["-c"]["--config"]("Config path") |
             clara::detail::Opt(style_opt, "style")["-s"]["--style"]("Style path") |
             clara::detail::Opt(
                 log_level,
                 "trace|debug|info|warning|error|critical|off")["-l"]["--log-level"]("Log level") |
             clara::detail::Opt(bar_id, "id")["-b"]["--bar"]("Bar id");
  auto res = cli.parse(clara::detail::Args(argc, argv));
  if (!res) {
    spdlog::error("Error in command line: {}", res.errorMessage());
    return 1;
  }
  if (show_help) {
    std::cout << cli << '\n';
    return 0;
  }
  if (show_version) {
    std::cout << "Waybar v" << VERSION << '\n';
    return 0;
  }
  if (!log_level.empty()) {
    spdlog::set_level(spdlog::level::from_str(log_level));
  }
  gtk_app = Gtk::Application::create(argc, argv, "fr.arouillard.waybar",
                                     Gio::APPLICATION_HANDLES_COMMAND_LINE);

  // Initialize Waybars GTK resources with our custom icons
  auto theme = Gtk::IconTheme::get_default();
  theme->add_resource_path("/fr/arouillard/waybar/icons");

  gdk_display = Gdk::Display::get_default();
  if (!gdk_display) {
    throw std::runtime_error("Can't find display");
  }
  if (!GDK_IS_WAYLAND_DISPLAY(gdk_display->gobj())) {
    throw std::runtime_error("Bar need to run under Wayland");
  }
  wl_display = gdk_wayland_display_get_wl_display(gdk_display->gobj());
  config.load(config_opt);
  if (!portal) {
    portal = std::make_unique<waybar::Portal>();
  }
  m_cssFile = getStyle(style_opt);
  setupCss(m_cssFile);
  m_cssReloadHelper = std::make_unique<CssReloadHelper>(m_cssFile, [&]() { setupCss(m_cssFile); });
  portal->signal_appearance_changed().connect([&](waybar::Appearance appearance) {
    auto css_file = getStyle(style_opt, appearance);
    setupCss(css_file);
  });

  auto m_config = config.getConfig();
  if (m_config.isObject() && m_config["reload_style_on_change"].asBool()) {
    m_cssReloadHelper->monitorChanges();
  } else if (m_config.isArray()) {
    for (const auto& conf : m_config) {
      if (conf["reload_style_on_change"].asBool()) {
        m_cssReloadHelper->monitorChanges();
        break;
      }
    }
  }

  bindInterfaces();
  gtk_app->hold();
  gtk_app->run();
  m_cssReloadHelper.reset();  // stop watching css file
  bars.clear();
  return 0;
}

*/
