project(
    'waybar', 'cpp', 'c',
    version: '4.0.0',
    license: 'MIT',
    meson_version: '>= 1.3.0',
    default_options : [
        'cpp_std=c++23',
        'buildtype=release',
        'default_library=static'
    ],
)

compiler = meson.get_compiler('cpp')

cpp_args = []
cpp_link_args = []

git = find_program('git', native: true, required: false)

if not git.found()
    add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
else
    git_path = run_command(git, 'rev-parse', '--show-toplevel', check: false).stdout().strip()
    if meson.project_source_root() == git_path
        git_commit_hash = run_command(git, 'describe', '--always', '--tags', check: false).stdout().strip()
        git_branch = run_command(git, 'rev-parse', '--abbrev-ref', 'HEAD', check: false).stdout().strip()
        version = '"@0@ (branch \'@1@\')"'.format(git_commit_hash, git_branch)
        add_project_arguments('-DVERSION=@0@'.format(version), language: 'cpp')
    else
        add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
    endif
endif

add_global_arguments(cpp_args, language : 'cpp')
add_global_link_arguments(cpp_link_args, language : 'cpp')

is_linux = host_machine.system() == 'linux'
is_dragonfly = host_machine.system() == 'dragonfly'
is_freebsd = host_machine.system() == 'freebsd'
is_netbsd = host_machine.system() == 'netbsd'
is_openbsd = host_machine.system() == 'openbsd'

fmt = dependency('fmt', version : ['>=8.1.1'], fallback : ['fmt', 'fmt_dep'])
giounix = dependency('gio-unix-2.0')
gtkmm = dependency('gtkmm-4.0', version : ['>=4.16.0'])
gtk_layer_shell = dependency('gtk4-layer-shell-0',
                             version : ['>=1.0.3'],
                             default_options : ['introspection=false', 'vapi=false'],
                             fallback : ['gtk4-layer-shell', 'gtk_layer_shell'])
jsoncpp = dependency('jsoncpp', version : ['>=1.9.6'], fallback : ['jsoncpp', 'jsoncpp_dep'])
spdlog = dependency('spdlog', version : ['>=1.10.0'], fallback : ['spdlog', 'spdlog_dep'], default_options : ['external_fmt=enabled', 'std_format=disabled', 'tests=disabled'])
wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')
conf_data = configuration_data()
conf_data.set('prefix', prefix)

add_project_arguments('-DSYSCONFDIR="@0@"'.format(prefix / sysconfdir), language : 'cpp')

subdir('protocol')
#subdir('resources/icons')

inc_dirs = ['include']

src_files = files(
          'src/main.cpp',
          'src/config.cpp',
          'src/client.cpp',
          'src/supply/css_reload_helper.cpp',
          'src/supply/portal.cpp')

app_resources = []

executable(
    'waybar',
    [src_files, app_resources],
    dependencies: [
        fmt,
        spdlog,
        jsoncpp,
        gtkmm,
        gtk_layer_shell,
        client_protos,
        wayland_client,
        wayland_protos
    ],
    include_directories: inc_dirs,
    install: true,
)

install_data(
    'resources/config.jsonc',
    'resources/style.css',
    install_dir: sysconfdir / 'xdg/waybar'
)
