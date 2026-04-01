#pragma once

#include <giomm/filemonitor.h>

struct pollfd;

namespace waybar {
class CssReloadHelper {
 public:
  CssReloadHelper(std::string cssFile, std::function<void()> callback);
  virtual ~CssReloadHelper() = default;
  virtual void monitorChanges();

 private:
  std::string m_cssFile;
  std::function<void()> m_callback;
  std::vector<std::tuple<Glib::RefPtr<Gio::FileMonitor>>> m_fileMonitors;
  // Methods
  virtual std::string findPath(const std::string& filename,
                               std::function<std::optional<std::string>(const std::vector<std::string>& names)> fallbackFind = nullptr);
  virtual std::string getFileContents(const std::string& filename);
  void handleFileChange(Glib::RefPtr<Gio::File> const& file,
                        Glib::RefPtr<Gio::File> const& other_type,
                        Gio::FileMonitor::Event event_type);
  bool handleInotifyEvents(int fd);
  std::vector<std::string> parseImports(const std::string& cssFile);
  void parseImports(const std::string& cssFile, std::unordered_map<std::string, bool>& imports);
  bool watch(int inotifyFd, pollfd* pollFd);
  void watchFiles(const std::vector<std::string>& files);
};
}  // namespace waybar
