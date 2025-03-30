plugin_sources = [
  'wavelan.c',
  'wi_bsd.c',
  'wi_common.c',
  'wi_darwin.c',
  'wi_linux.c',
  'wi.h',
  xfce_revision_h,
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

plugin_lib = shared_module(
  'wavelan',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-wavelan-plugin'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libm,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'wavelan.desktop.in',
  output: 'wavelan.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
