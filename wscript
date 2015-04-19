#! /usr/bin/env python
# encoding: utf-8
# Jordi Rovira i Bonet

import sys
import shutil
import os
from waflib.Context import Context
from waflib.Build import BuildContext
from waflib.Configure import ConfigurationContext
from waflib.Scripting import Dist
from waflib import Utils, Errors, Logs
import csv
import socket
import waflib
import glob
import datetime
import string

import crosabuild

APPNAME = 'craft'
VERSION = '0.0'


top = '.'
out = 'build/waf'


#--------------------------------------------------------------------------------------------------
# Gather configuration options
#--------------------------------------------------------------------------------------------------
def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.add_option( '--platform', action='store',
                    help='Target platform ("Linux", "Windows", "OSX" or "Android")')
    opt.add_option( '--arch', action='store',
                    help='Target processor architecture ("x86_32", "x86_64", "armv7")')
    opt.add_option( '--download_proxy', action='store',
                    help='Path to a folder to look for files before downloading from internet.')
    opt.add_option( '--test', action='store_true', help='Run the automated tests after building')


#--------------------------------------------------------------------------------------------------
# Preconfigure the build script
#--------------------------------------------------------------------------------------------------
def preconfigure(ctx):
    crosabuild.preconfigure(ctx)


#--------------------------------------------------------------------------------------------------
# Configure the build script
#--------------------------------------------------------------------------------------------------
def configure(ctx):

    ctx.env.CROSA_VERSION = VERSION

    # os platform
    ctx.env.CROSA_PLATFORM_OS = ' Crosa-Platform-OS-'+ctx.env.TARGETPLATFORM

    if ctx.env.TARGETPLATFORM=='Android':
        crosabuild.configure_android(ctx)

    if ctx.env.TARGETPLATFORM=='Windows':
        ctx.check(features='cxx cxxprogram', msg='checking for system libraries',
                  lib=['WinMM', 'imm32', 'version', 'uuid', 'Ole32', 'OleAut32', 'gdi32', 'User32',
                       'Shell32', 'Comdlg32', 'OpenGL32', 'GLU32'], cflags=['-Wall'],
                  uselib_store='GL')

    elif ctx.env.TARGETPLATFORM=='OSX':
        ctx.env.FRAMEWORK_GL = ['OpenGL','Foundation','ForceFeedback','AppKit','Cocoa','Carbon',
                                'CoreAudio','AudioToolbox','AudioUnit','IOKit']

    elif ctx.env.TARGETPLATFORM=='Linux':
        ctx.check(features='cxx cxxprogram', lib=['dl'], cflags=['-Wall'], uselib_store='GL')

    # Common compilation flags
    if ctx.env.CXX_NAME=='msvc':
        common_CFLAGS                   = ['/nologo', '/EHsc', '/W3', '/wd4996', '/wd4819']
        common_CXXFLAGS                 = ['/nologo', '/EHsc', '/W3', '/WX', '/wd4996', '/wd4819']
        common_LINKFLAGS                = ['/nologo', '/MANIFEST', '/LARGEADDRESSAWARE']

    elif ctx.env.TARGETPLATFORM=='OSX':
        #presume 'gcc' or compatible
        common_CFLAGS                   = ['-Wall', '-I/usr/X11/include']
        common_CXXFLAGS                 = ['-Wall', '-std=c++11', '-I/usr/X11/include', '-fPIC']
        common_LINKFLAGS                = ['-L/usr/X11/lib','-framework', 'OpenGL']

        if ctx.env.TARGETARCH=='x86_32':
            common_CFLAGS += ['-m32']
            common_CXXFLAGS += ['-m32']
            common_LINKFLAGS += ['-m32']
        elif ctx.env.TARGETARCH=='x86_64':
            common_CFLAGS += ['-m64']
            common_CXXFLAGS += ['-m64']
            common_LINKFLAGS += ['-m64']

    else:
        #presume 'gcc' or compatible
        common_CFLAGS                   = []
        common_CXXFLAGS                 = ['-Wall', '-std=c++0x', '-fPIC']
        common_LINKFLAGS                = []
        if ctx.env.TARGETPLATFORM!='Windows':
            common_CXXFLAGS         += ['-Werror']

        if ctx.env.TARGETARCH=='x86_32':
            common_CFLAGS += ['-m32']
            common_CXXFLAGS += ['-m32']
            common_LINKFLAGS += ['-m32']
        elif ctx.env.TARGETARCH=='x86_64':
            common_CFLAGS += ['-m64']
            common_CXXFLAGS += ['-m64']
            common_LINKFLAGS += ['-m64']

        if ctx.env.TARGETPLATFORM=='Windows':
            common_CXXFLAGS         += ['-mwindows']
            common_LINKFLAGS        += ['-Wl,-subsystem,windows' ]

    # Variants
    default_env = ctx.env.derive()

    # Debug variant
    ctx.setenv('debug',default_env.derive())
    if ctx.env.CXX_NAME=='msvc':
        ctx.env.CFLAGS                  = common_CFLAGS + ['/GS', '/Zi', '/Od', '/MDd']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['/GS', '/Zi', '/Od', '/MDd']
        ctx.env.LINKFLAGS               = common_LINKFLAGS + ['/DEBUG','/ignore:4099']
    else:
        #presume 'gcc' or compatible
        ctx.env.CFLAGS                  = common_CFLAGS + ['-g', '-O0']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['-g', '-O0', '-Woverloaded-virtual', '-finline-functions']
        ctx.env.LINKFLAGS               = common_LINKFLAGS

    # Profile variant
    ctx.setenv('profile',default_env.derive())
    if ctx.env.CXX_NAME=='msvc':
        ctx.env.CFLAGS                  = common_CFLAGS + ['/GS', '/Zi', '/Ox', '/MD']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['/GS', '/Zi', '/Ox', '/MD']
        ctx.env.LINKFLAGS               = common_LINKFLAGS
    else:
        #presume 'gcc' or compatible
        ctx.env.CFLAGS                  = common_CFLAGS + ['-g', '-O3', '-DNDEBUG']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['-g', '-O3', '-DNDEBUG']
        ctx.env.LINKFLAGS               = common_LINKFLAGS

    # Release variant
    ctx.setenv('release',default_env.derive())
    if ctx.env.CXX_NAME=='msvc':
        ctx.env.CFLAGS                  = common_CFLAGS + ['/Ox', '/MD', '/D_SECURE_SCL=0', '/DNDEBUG']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['/Ox', '/MD', '/D_SECURE_SCL=0', '/DNDEBUG']
        ctx.env.LINKFLAGS               = common_LINKFLAGS
    else:
        #presume 'gcc' or compatible
        ctx.env.CFLAGS                  = common_CFLAGS + ['-O3', '-DNDEBUG']
        ctx.env.CXXFLAGS                = common_CXXFLAGS + ['-O3', '-DNDEBUG']
        ctx.env.LINKFLAGS               = common_LINKFLAGS


#--------------------------------------------------------------------------------------------------
# Build the source code
#--------------------------------------------------------------------------------------------------
def build(ctx):

    if not ctx.variant:
        ctx.fatal('The available build configurations are:\n'
                  '\t waf debug : unoptimised, with symbols\n'
                  '\t waf profile : optimised, with symbols\n'
                  '\t waf release : optimised and without symbols\n')

    ctx.env.CROSA_ROOT = ctx.path.abspath()

    ctx.shlib(
        source   = 'source/craft.cpp source/context.cpp source/target.cpp source/platform.cpp source/compiler.cpp',
        target   = 'craft-core',
        defines  = ' CRAFTCOREI_BUILD ',
        includes = 'source',
        )

    ctx.program(
        source   = 'source/main.cpp',
        target   = 'craft',
        use      = ' craft-core',
        lib      = ' dl ',
        includes = 'source',
        linkflags  = '-Wl,-rpath,$ORIGIN'
        )



#--------------------------------------------------------------------------------------------------
#
#--------------------------------------------------------------------------------------------------
def prebuild(ctx):
    crosabuild.prebuild(ctx)


class BuildDebugContext( Context ):
    cmd = 'debug'
    variant = 'debug'
    fun = 'prebuild'

class BuildReleaseContext( Context ):
    cmd = 'release'
    variant = 'release'
    fun = 'prebuild'

class BuildProfileContext( Context ):
    cmd = 'profile'
    variant = 'profile'
    fun = 'prebuild'





