#pragma once

#include "config.hpp"
#include "supply/css_reload_helper.hpp"
#include "supply/portal.hpp"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <gdk/wayland/gdkwayland.h>
#include <gtkmm/application.h>
#include <gtkmm/cssprovider.h>

typedef struct zxdg_output_manager_v1* xdg_output_manager_ptr;
typedef struct zwp_idle_inhibit_manager_v1* idle_inhibit_manager_ptr;

namespace waybar {
class Client {
 public:
  static Client* inst();
  int main(int argc, char* argv[]);
  void reset();

 private:
  Client() = default;
  Glib::RefPtr<Gtk::Application> m_gtk_app_;
  std::string m_bar_id_;
  Config m_config_;
  std::string m_cssFile_;
  Glib::RefPtr<Gtk::CssProvider> m_css_provider_;
  std::unique_ptr<CssReloadHelper> m_css_reload_helper_;
  Glib::RefPtr<Gdk::Display> m_gdk_display_;
  Glib::RefPtr<Gio::ListModel> m_monitors_;
  std::unique_ptr<Portal> m_portal_;
  struct wl_display* m_wl_display_{nullptr};
  struct wl_registry* m_registry_{nullptr};
  xdg_output_manager_ptr m_xdg_output_manager_{nullptr};
  idle_inhibit_manager_ptr m_idle_inhibit_manager_{nullptr};

  // Methods
  void bindInterfaces();
  std::vector<Json::Value> getOutputCfg(const Glib::RefPtr<Gdk::Monitor>& monitor);
  const std::string getStyle(const std::string& style,
                             std::optional<Appearance> appearance = std::nullopt);
  void setupCss(const std::string& css_file);
  void setXdgOutputManager(const xdg_output_manager_ptr val);
  void setIdleInhibitManager(const idle_inhibit_manager_ptr val);
  // Handlers
  void onMonAdd(const Glib::RefPtr<Gdk::Monitor> monitor);
  void onMonRemove(const Glib::RefPtr<Gdk::Monitor> monitor);
  void onMonRemoveDeffered(const Glib::RefPtr<Gdk::Monitor> monitor);
  static void onOutputDone(void*, struct zxdg_output_v1*); 
  static void onWlGlobal(void* data, struct wl_registry* registry, uint32_t name,
                         const char* interface, uint32_t version);
  static void onWlGlobalRemove(void* data, struct wl_registry* registry, uint32_t name);

};
} // namespace waybar

/*
#pragma once

//#include <fmt/format.h>
//#include <gdk/gdk.h>
//#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include <gdk/wayland/gdkwayland.h>

#include "xdg-output-unstable-v1-client-protocol.h"


#include "bar.hpp"
//#include "config.hpp"
//#include "util/css_reload_helper.hpp"
//#include "util/portal.hpp"

struct zwp_idle_inhibitor_v1;
struct zwp_idle_inhibit_manager_v1;

namespace waybar {

class Client {
 public:
  static Client* inst();
  int main(int argc, char* argv[]);
  void reset();

  Glib::RefPtr<Gtk::Application> gtk_app;
  Glib::RefPtr<Gdk::Display> gdk_display;
  struct wl_display* wl_display = nullptr;
  struct wl_registry* registry = nullptr;
  struct zxdg_output_manager_v1* xdg_output_manager = nullptr;
  struct zwp_idle_inhibit_manager_v1* idle_inhibit_manager = nullptr;
  std::vector<std::unique_ptr<Bar>> bars;
//  Config config;
  std::string bar_id;

 private:
  Client() = default;
//  const std::string getStyle(const std::string& style, std::optional<Appearance> appearance);
  void bindInterfaces();
//  void handleOutput(struct waybar_output& output);
  auto setupCss(const std::string& css_file) -> void;
//  struct waybar_output& getOutput(void*);
//  std::vector<Json::Value> getOutputConfigs(struct waybar_output& output);

  static void handleGlobal(void* data, struct wl_registry* registry, uint32_t name,
                           const char* interface, uint32_t version);
  static void handleGlobalRemove(void* data, struct wl_registry* registry, uint32_t name);
  static void handleOutputDone(void*, struct zxdg_output_v1*);
  static void handleOutputName(void*, struct zxdg_output_v1*, const char*);
  static void handleOutputDescription(void*, struct zxdg_output_v1*, const char*);
  void handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor);
  void handleMonitorRemoved(Glib::RefPtr<Gdk::Monitor> monitor);
  void handleDeferredMonitorRemoval(Glib::RefPtr<Gdk::Monitor> monitor);

  Glib::RefPtr<Gtk::StyleContext> style_context_;
  Glib::RefPtr<Gtk::CssProvider> css_provider_;
//  std::unique_ptr<Portal> portal;
//  std::list<struct waybar_output> outputs_;
//  std::unique_ptr<CssReloadHelper> m_cssReloadHelper;
  std::string m_cssFile;
  sigc::connection monitor_added_connection_;
  sigc::connection monitor_removed_connection_;
};

}  // namespace waybar
*/
