#pragma once

#include <fmt/format.h>
#include <giomm/dbusproxy.h>

namespace waybar {

enum class Appearance {
  UNKNOWN = 0,
  DARK = 1,
  LIGHT = 2,
};

class Portal : private Gio::DBus::Proxy {
 public:
  Portal();
  void refreshAppearance();
  Appearance getAppearance();

  typedef sigc::signal<void(Appearance)> type_signal_appearance_changed;
  type_signal_appearance_changed signal_appearance_changed() { return m_signal_appearance_changed_; }

 private:
  type_signal_appearance_changed m_signal_appearance_changed_;
  Appearance m_current_mode_;
  void on_signal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                 const Glib::VariantContainerBase& parameters);
};

}  // namespace waybar

template <>
struct fmt::formatter<waybar::Appearance> : formatter<fmt::string_view> {
  // parse is inherited from formatter<string_view>.
  auto format(waybar::Appearance c, format_context& ctx) const;
};
