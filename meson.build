project('BerserkerQuake2', 'c')

# Dependencies
dep_gl = dependency('GL')
dep_vorbis = dependency('vorbis')
dep_vorbisfile = dependency('vorbisfile')
dep_sdl = dependency('SDL2')
dep_png = dependency('libpng')
dep_minizip = dependency('minizip')
deplist = [dep_gl, dep_vorbis, dep_vorbisfile, dep_sdl, dep_png, dep_minizip]

# Game-specific code
shared_library('game', 'game.c',
    name_prefix: '',
    link_args: ['-lm'], install_dir: get_option('bindir') + 'baseq2')

# The berserkerq2 executable depends on some headers in the repo
incdir = include_directories('include')
executable('berserkerq2',
    'main.c', 'unpak.c',
    dependencies: deplist,
    link_args: ['-ljpeg', '-lm'],
    include_directories: incdir)
