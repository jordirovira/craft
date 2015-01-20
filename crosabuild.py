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
from waflib.Task import Task
from waflib import Utils, Errors, Logs
import csv
import socket
import waflib
import glob
import datetime
import string
import time
import subprocess
import platform
from xml.sax.saxutils import escape



#--------------------------------------------------------------------------------------------------
#
#--------------------------------------------------------------------------------------------------
def find_7zip( ctx ):
        paths = []
        if ctx.env.HOSTPLATFORM=='Windows':
                if 'ProgramFiles' in os.environ:
                        paths += [  os.environ['ProgramFiles']+'\\7-Zip' ]
                if 'ProgramFiles(x86)' in os.environ:
                        paths += [ os.environ['ProgramFiles(x86)']+'\\7-Zip']
        else:
            paths += ['/usr/bin' ]
            paths += ['/usr/local/bin' ]

        try:
                waflib.Configure.find_program( ctx, '7za', path_list=paths )
        except:
                waflib.Configure.find_program( ctx, '7z', var='7ZA', path_list=paths )


#--------------------------------------------------------------------------------------------------
#
#--------------------------------------------------------------------------------------------------
def find_hg( ctx ):
    waflib.Configure.find_program( ctx, 'hg' )


#--------------------------------------------------------------------------------------------------
#
#--------------------------------------------------------------------------------------------------
def find_doxygen( ctx ):
	paths = []
	if ctx.env.HOSTPLATFORM=='Windows':
		paths += [  os.environ["ProgramFiles"]+'\\doxygen\\bin',
					os.environ["ProgramFiles(x86)"]+'\\doxygen\\bin',
					'/usr/bin' ]
	try:
		waflib.Configure.find_program( ctx, 'doxygen', path_list=paths )
	except:
		pass



#--------------------------------------------------------------------------------------------------
# This is used to copy a file like in a task
#--------------------------------------------------------------------------------------------------
def copy_file( bld, pathToFile, file ):
	bld(
		rule   = copy,
		source = [pathToFile+os.sep+file],
		target = file,
		name = 'copy',
		color = 'CYAN'
		)

def copy(task):
	for s in task.inputs:
		src = s.abspath()
		tgt = task.outputs[0].abspath()
		shutil.copy( src, tgt )
	return False



#--------------------------------------------------------------------------------------------------
# This is used to copy the dynamic libraries next to the generated targets using
# crosa, in the platforms that require it. We'll bind this method to the build context
#--------------------------------------------------------------------------------------------------
def copy_crosa_dynamic_libraries( bld ):

	if sys.platform=='win32':

# 		if bld.env.LIBPATH_DEVIL!=[]:
# 			pathToLibs = os.path.relpath( bld.env.LIBPATH_DEVIL[0], bld.path.abspath() )
# 			copy_file(bld,pathToLibs,'DevIL.dll')

# we use a static version of the lib now.
#		if bld.env.LIBPATH_FBX!=[]:
#			pathToLibs = os.path.relpath( bld.env.LIBPATH_FBX[0], bld.path.abspath() )
#			copy_file(bld,pathToLibs,'fbxsdk-2013.3.dll')

		# MSVC Runtime, if necessary
		if bld.variant=='release' and bld.env.CXX_NAME=='msvc':

			class copy_msvc_task(Task):
				outputPath = ''
				redistPath = ''
				dlls = []

				def __str__(self):
					return 'copy msvc runtime to '+self.outputPath+'\n'

				def prepare(self,bld):
					if self.env.MSVC_VERSION==10.0:
						redistPath = os.path.abspath( os.path.dirname( self.env.CXX[0] ) )
						if self.env.TARGETARCH=='x86_32':
							redistPath +='\\..\\redist\\x86'
						else:
							redistPath +='\\..\\..\\redist\\x64'
						redistPath +='\\Microsoft.VC100.CRT\\'
						self.dlls = ['msvcp100.dll','msvcr100.dll']

					elif self.env.MSVC_VERSION==11.0:
						redistPath = os.path.abspath( os.path.dirname( self.env.CXX[0] ) )
						if self.env.TARGETARCH=='x86_32':
							redistPath +='\\..\\redist\\x86'
						else:
							redistPath +='\\..\\..\\redist\\x64'
						redistPath +='\\Microsoft.VC110.CRT\\'
						self.dlls = ['msvcp110.dll','msvcr110.dll']

					self.redistPath=redistPath

					outputs = []
					for f in self.dlls:
						outputs += [bld.path.get_bld().make_node(f)]
					self.set_outputs( outputs );

				def run(self):
					success = False
					#try:
					for f in self.dlls:
						if not os.path.exists( self.outputPath+f ):
							shutil.copy( self.redistPath+f, self.outputPath )
					success = True
					#except:
					#	pass

					return not success

			copy_msvc_task = waflib.Task.always_run(copy_msvc_task)
			bld.add_group()
			run_1 = copy_msvc_task(env=bld.env)
			run_1.outputPath = waflib.Context.out_dir + "\\" + bld.variant + "\\" + bld.path.srcpath() + "\\"
			run_1.prepare(bld)
			run_1.color = 'YELLOW'
			bld.add_to_group( run_1 )
			bld.add_group()


def copy_crosa_taula_data( bld, subdir='' ):

	pathToData = os.path.relpath( bld.env.CROSA_ROOT, bld.path.abspath()+subdir )
	for f in glob.glob(bld.env.CROSA_ROOT+'/Data/System/*.crosa'):
		copy_file(bld,pathToData,'Data/System/'+os.path.basename(f))


#--------------------------------------------------------------------------------------------------
#
#--------------------------------------------------------------------------------------------------
def prebuild(ctx):

	try:
		os.makedirs('build/waf')
	except OSError:
		pass
	ctx.logger = Logs.make_logger('build/waf/prebuild.log', 'prebuild')

	# Preconfiguration to decide target name
	out = 'build/waf/'
	preconfigureContext = PreconfigurationContext()
	preconfigureContext.out_dir = out
	preconfigureContext.options = ctx.options
	preconfigureContext.execute()

	# Real configuration
	out = 'build/waf/'+preconfigureContext.env.TARGETNAME
	configureContext = ConfigurationContext()
	configureContext.out_dir = out
	configureContext.init_dirs()
	configureContext.options = ctx.options
	configureContext.env = preconfigureContext.env
	configureContext.env.VARIANT = ctx.variant

	try:
		configureContext.execute()

	except Exception as e:
		ctx.start_msg("configuring toolchain ")
		ctx.end_msg("failed", color="RED")
		print e
		pass

	else:
		ctx.start_msg("configuring toolchain ")
		ctx.end_msg("ok", color="GREEN")

		# build
		buildContext = CustomBuildContext()
		buildContext.options = ctx.options
		buildContext.variant = ctx.variant
		buildContext.cmd = ctx.variant
		buildContext.execute()


#--------------------------------------------------------------------------------------------------
# Preconfigure
#--------------------------------------------------------------------------------------------------
def preconfigure(ctx):
	identify_platforms(ctx)
	ctx.load('compiler_cxx')
        #ctx.load('compiler_c')

	# mix all strings
	targetName = ctx.env.TARGETPLATFORM+'-'+ctx.env.TARGETARCH+'-'+ctx.env.CXX_NAME
	if ctx.env.CXX_NAME=='msvc':
		targetName += str(ctx.env.MSVC_VERSION)
	elif ctx.env.CXX_NAME=='gcc':
		targetName += '.'.join(ctx.env.CC_VERSION)
	ctx.env.TARGETNAME=targetName

class PreconfigurationContext( ConfigurationContext ):
	cmd = 'preconfigure'
	fun = 'preconfigure'


#--------------------------------------------------------------------------------------------------
# Identify the host and target platforms and architectures.
#--------------------------------------------------------------------------------------------------
PLATFORMS = ['Linux','Windows','OSX','Android','iOS']
ARCHITECTURES = ['x86_32','x86_64','arm7']

def identify_platforms( ctx ):

	if not hasattr(ctx,'env'):
		ctx.env =  waflib.ConfigSet.ConfigSet()

	if not hasattr(ctx,'options'):
		ctx.options = {}

	# Do it only once
	if ctx.env.HOSTPLATFORM==[]:

		hostPlatform = 'Unknown'
		if sys.platform=='win32' or sys.platform=='cygwin':
			hostPlatform='Windows'
		elif sys.platform=='linux2' or sys.platform=='linux3':
			hostPlatform='Linux'
		elif sys.platform=='darwin':
			hostPlatform='OSX'
		else:
			ctx.fatal('Unidentified platform ['+sys.platform+']')

		ctx.env.HOSTPLATFORM=hostPlatform
		ctx.env.TARGETPLATFORM=hostPlatform

		if ctx.options.platform:
			if ctx.options.platform not in PLATFORMS:
				ctx.fatal( "Error: Unsupported platform ["+ctx.options.platform+
										"]. Valid options are:"+str(PLATFORMS) )
			else:
				ctx.env.TARGETPLATFORM=ctx.options.platform

		ctx.msg( 'Host platform : ', hostPlatform )
		ctx.msg( 'Target platform : ', ctx.env.TARGETPLATFORM )

		# Processor architecture
		if ctx.options.arch:
			if ctx.options.arch not in ARCHITECTURES:
				ctx.fatal( "Error: Unsupported architecture ["+ctx.options.arch+
										"]. Valid options are:"+str(ARCHITECTURES) )
			ctx.env.TARGETARCH=ctx.options.arch

			if sys.platform == 'win32':
				if ctx.options.arch=='x86_32':
					ctx.env['MSVC_TARGETS'] = 'x86'
				else:
					ctx.env['MSVC_TARGETS'] = 'x86_amd64'
		else:
			if sys.platform == 'win32':
				ctx.env.TARGETARCH = ctx.env['MSVC_TARGETS']
				if ctx.env.TARGETARCH==[]:
					if platform.architecture()[0]=='64bit':
						ctx.env.TARGETARCH = 'x86_64'
						ctx.env['MSVC_TARGETS'] = 'x86_amd64'
					else:
						ctx.env.TARGETARCH = 'x86_32'
						ctx.env['MSVC_TARGETS'] = 'x86'

			elif ctx.env.TARGETPLATFORM == 'Android':
				ctx.env.TARGETARCH = 'arm7'

			else:
				if platform.architecture()[0]=='64bit':
					ctx.env.TARGETARCH = 'x86_64'
				else:
					ctx.env.TARGETARCH = 'x86_32'

		ctx.msg( 'Target architecture : ', ctx.env.TARGETARCH )


#--------------------------------------------------------------------------------------------------
# remove duplicated elements from an array
#--------------------------------------------------------------------------------------------------
def make_unique(seq, idfun=None):
	if idfun is None:
		def idfun(x): return x
	seen = {}
	result = []
	for item in seq:
		marker = idfun(item)
		if marker in seen: continue
		seen[marker] = 1
		result.append(item)
	return result



#--------------------------------------------------------------------------------------------------
# Build with test context
#--------------------------------------------------------------------------------------------------
class CustomBuildContext(BuildContext):

	tests = []

	#------------------------------------------------------------------------------------------
	def new_suite_execution( self ):
		tests = []


	#------------------------------------------------------------------------------------------
	def add_test_result( self, name, success, metrics, out='', err='' ):
		entry = {}
		entry['name'] = name
		entry['success'] = success
		entry['metrics'] = metrics
		entry['out'] = out
		entry['err'] = err
		self.tests.append( entry )


	#------------------------------------------------------------------------------------------
	def report_csv( self, name, success, output, stdout, stderr ):
		execution = {}

		if success and os.path.exists( output ):

			csvReader = csv.reader( open(output, 'rb') )
			for metric in csvReader:
				if metric[0]=='scalar':
					metricName = metric[1]
					value = metric[2]
					execution[ metricName.strip() ] = string.atof(value)

		self.add_test_result( name, success, execution, stdout, stderr )


	#------------------------------------------------------------------------------------------
	def test( self, name, command, precommand='' ):

		class run_test_task(Task):

			resultPath = ''
			command=''
			context=None
			testName=''

			def __str__(self):
				return 'run test      :'+self.testName+'\n'

			def run(self):
				output = self.resultPath+'results.csv'
				command = self.command+' --output '+output

				child = subprocess.Popen( precommand+' '+self.resultPath+command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				out,err = child.communicate()
				success = child.returncode==0

				self.context.report_csv( name, success, output, out, err )
				return False


		run_test_task = waflib.Task.always_run(run_test_task)
		self.add_group()
		run_1 = run_test_task(env=self.env)
		run_1.testName = name
		run_1.command = command
		run_1.resultPath = waflib.Context.out_dir + "/" + self.variant + "/" + self.path.srcpath() + "/"
		run_1.color = 'YELLOW'
		run_1.context = self
		self.add_to_group( run_1 )
		self.add_group()


	#------------------------------------------------------------------------------------------
	def report_tests( self ):

		class run_report_test_task(Task):

			resultPath = ''
			command=''
			context=None

			def __str__(self):
				return 'writing test reports at '+os.path.normpath(self.resultPath+'/../junit_results')+'\n'

			def run(self):
				# delete any previous test results
				try:
					shutil.rmtree(self.resultPath+'/../junit_results')
				except:
					pass

				# make sure the results folder exists
				try:
					os.makedirs( self.resultPath+'/../junit_results' )
				except:
					pass

				testIndex=0
				for test in self.context.tests:

					# test log result
					fileName = 'junit_results_'+test['name']+'_execution.xml'
					file = open(self.resultPath+'/../junit_results/'+fileName, 'w')
					file.write('<testsuite>\n')

					file.write('  <testcase classname="'+test['name']+'" name="execution" time="1">\n')
					if not test['success']:
						if not 'failure' in test.keys():
							file.write('    <failure type="general">Unknown error.</failure>\n')
						else:
							file.write('    <failure type="general">'+escape(test['failure'])+'</failure>\n')
					file.write('  </testcase>\n')

					file.write('  <system-out>\n')
					file.write( escape( test['out'] ) )
					file.write('  </system-out>\n')

					file.write('  <system-err>\n')
					file.write( escape( test['err'] ) )
					file.write('  </system-err>\n')

					file.write('</testsuite>\n')
					file.close()

					# a file for every test metric
					for name,value in test['metrics'].items():
						fileName = 'junit_results_'+test['name']+'_'+name+'.xml'
						file = open(self.resultPath+'/../junit_results/'+fileName, 'w')
						file.write('<testsuite>\n')

						file.write('  <testcase classname="'+test['name']+'" name="'+name+'" time="'+str(value)+'">\n')
						file.write('  </testcase>\n')

						file.write('  <system-out>\n')
						file.write('  </system-out>\n')

						file.write('  <system-err>\n')
						file.write('  </system-err>\n')

						file.write('</testsuite>\n')
						file.close()
						testIndex += 1

				return False


		run_report_test_task = waflib.Task.always_run(run_report_test_task)
		self.add_group()
		run_1 = run_report_test_task(env=self.env)
		run_1.resultPath = waflib.Context.out_dir + "/"
		run_1.color = 'BLUE'
		run_1.context = self
		self.add_to_group( run_1 )
		self.add_group()


	#------------------------------------------------------------------------------------------
	def test_android( self, name, process, activity ):

		self.start_msg("running "+name)

		success = False

		try:
			self.cmd_and_log(
				[ '/home/jordi/android-sdk-linux/platform-tools/adb', 'shell', 'am', 'start', '-W', '-a',
				  'android.intent.action.MAIN', '-n', process+'/.'+activity ],
				output=waflib.Context.STDOUT,
				quiet=None )

			# wait for the process to end
			finished = False
			while not finished:
					time.sleep(5)
					processes = self.cmd_and_log(
						[ '/home/jordi/android-sdk-linux/platform-tools/adb', 'shell', 'ps' ],
						output=waflib.Context.STDOUT,
						quiet=waflib.Context.BOTH )
					if string.find( processes, process )==-1:
							finished = True
					else:
							print 'waiting...'

			result = self.cmd_and_log(
						[ '/home/jordi/android-sdk-linux/platform-tools/adb', 'shell', 'cat', '/sdcard/results.csv' ],
						output=waflib.Context.STDOUT,
						quiet=waflib.Context.BOTH )

			success = True

		except:
			success = False

		execution = {}

		if success:
			self.end_msg("ok","GREEN")

			csvReader = csv.reader( result.split('\n') )
			for metric in csvReader:
				if len(metric)>0 and metric[0]=='scalar':
					metricName = metric[1]
					value = metric[2]
					execution[ metricName.strip() ] = string.atof(value)
		else:
			self.end_msg("failed","RED")

		self.add_test_result( name, success, execution )


	#------------------------------------------------------------------------------------------
	def valgrind( self, name, command ):

		class run_test_task(Task):

			resultPath = ''
			command=''
			context=None
			testName=''

			def __str__(self):
				return 'valgrind test :'+self.testName+'\n'

			def run(self):

				output = self.resultPath+'valgrind.output'
				success = True
				lines = []
				out = ''
				err = ''
				try:
					try:
						os.remove( output )
					except:
						pass

					command = 'valgrind --log-file='+output+' ' + self.resultPath + self.command
					child = subprocess.Popen( (command).split(' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
					out,err = child.communicate()
					success = child.returncode==0

					file = open(output, 'r')
					lines = file.readlines()
				except:
					success = False

				execution = {}

				if success:
					mem_allocs = 0
					mem_alloc_bytes = 0
					mem_lost = 0
					mem_possibly_lost = 0
					mem_errors = 0

					# We want to parse something like this:
					# ==10177== HEAP SUMMARY:
					# ==10177==     in use at exit: 388,449 bytes in 2,703 blocks
					# ==10177==   total heap usage: 1,089,078 allocs, 1,086,375 frees, 481,635,795 bytes allocated
					# ==10177==
					# ==10177== LEAK SUMMARY:
					# ==10177==    definitely lost: 1,093 bytes in 16 blocks
					# ==10177==    indirectly lost: 219,638 bytes in 2,213 blocks
					# ==10177==      possibly lost: 134,326 bytes in 16 blocks
					# ==10177==    still reachable: 33,392 bytes in 458 blocks
					# ==10177==         suppressed: 0 bytes in 0 blocks
					# ==4403== For counts of detected and suppressed errors, rerun with: -v
					# ==4403== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 4 from 4)
					for line in lines:
						words = line.strip().split()
						if len(words)>8:
							if words[1]=="total" and words[2]=="heap":
								mem_allocs = string.atoi(words[4].replace(',',''))
								mem_alloc_bytes = string.atoi(words[8].replace(',',''))

						if len(words)>5:
							if words[1]=="definitely" and words[2]=="lost:":
								mem_lost = mem_lost + string.atoi(words[3].replace(',',''))
							if words[1]=="indirectly" and words[2]=="lost:":
								mem_lost = mem_lost + string.atoi(words[3].replace(',',''))
							if words[1]=="possibly" and words[2]=="lost:":
								mem_possibly_lost = string.atoi(words[3].replace(',',''))
							if words[1]=="ERROR" and words[2]=="SUMMARY:":
								mem_errors = string.atoi(words[3].replace(',',''))

					execution[ 'mem_allocs' ] = mem_allocs
					execution[ 'mem_alloc_bytes' ] = mem_alloc_bytes
					execution[ 'mem_lost' ] = mem_lost
					execution[ 'mem_possibly_lost' ] = mem_possibly_lost
					execution[ 'mem_errors' ] = mem_errors

				self.context.add_test_result( name, success, execution, out, err )
				return False


		run_test_task = waflib.Task.always_run(run_test_task)
		self.add_group()
		run_1 = run_test_task(env=self.env)
		run_1.testName = name
		run_1.command = command
		run_1.resultPath = waflib.Context.out_dir + "/" + self.variant + "/" + self.path.srcpath() + "/"
		run_1.color = 'YELLOW'
		run_1.context = self
		self.add_to_group( run_1 )
		self.add_group()



	#------------------------------------------------------------------------------------------
	def massif( self, name, command ):

		class run_test_task(Task):

			resultPath = ''
			command=''
			context=None
			testName=''

			def __str__(self):
				return 'massif test   :'+self.testName+'\n'

			def run(self):

				output = self.resultPath+'massif.output'
				success = True
				lines = []
				out = ''
				err = ''
				try:
					try:
						os.remove( output )
					except:
						pass

					command = 'valgrind --tool=massif --massif-out-file='+output+' ' + self.resultPath + self.command
					child = subprocess.Popen( (command).split(' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
					out,err = child.communicate()
					success = child.returncode==0

					file = open(output, 'r')
					lines = file.readlines()
				except:
					success = False

				execution = {}

				if success:
					max_heap = 0
					avr_heap = 0
					num_heaps = 0

					# We want to parse something like this:
					##-----------
					#snapshot=1
					##-----------
					#time=4052181843
					#mem_heap_B=2327409381
					#mem_heap_extra_B=137691
					#mem_stacks_B=0
					#heap_tree=empty
					for line in lines:
						words = line.strip().split('=')
						if len(words)>=2:
							if words[0]=="mem_heap_B":
								heap = string.atoi(words[1]) / 1024
								max_heap = max( max_heap, heap )
								avr_heap += heap
								num_heaps += 1

					if num_heaps>0:
						avr_heap /= num_heaps

					execution[ 'max_heap' ] = max_heap
					execution[ 'avr_heap' ] = avr_heap

				self.context.add_test_result( name, success, execution, out, err )
				return False


		run_test_task = waflib.Task.always_run(run_test_task)
		self.add_group()
		run_1 = run_test_task(env=self.env)
		run_1.testName = name
		run_1.command = command
		run_1.resultPath = waflib.Context.out_dir + "/" + self.variant + "/" + self.path.srcpath() + "/"
		run_1.color = 'GREEN'
		run_1.context = self
		self.add_to_group( run_1 )
		self.add_group()



#----------------------------------------------------------------------------------------------
# Android: configure the SDK paths
#----------------------------------------------------------------------------------------------
def configure_android( ctx ):

	if not hasattr(ctx,'env'):
		ctx.env =  waflib.ConfigSet.ConfigSet()

	# Do it only once
	if ctx.env.ANDROIDSDK==[]:

		if ctx.options:
			if ctx.options.android_sdk:
				ctx.env.ANDROIDSDK=ctx.options.android_sdk

			if ctx.options.android_ndk:
				ctx.env.ANDROIDNDK=ctx.options.android_ndk

		if ctx.env.ANDROIDSDK == []:
			# TODO: wrong: since ~is not expanded in osx at least.
			candidates=['~/android-sdk-linux',
						'~/android-sdk-windows',
						'~/android-sdk-osx',
						'~/android-sdk']
			for c in candidates:
				fullPath = os.path.abspath( os.path.expanduser(c) )
				if os.path.isdir(fullPath):
					ctx.env.ANDROIDSDK = fullPath
					break
		if ctx.env.ANDROIDSDK == []:
			ctx.fatal('Android SDK not found. Try using the --android_sdk option.')
		else:
			ctx.msg( 'Android SDK : ', ctx.env.ANDROIDSDK )


		if ctx.env.ANDROIDNDK == []:
			candidates=['~/android-ndk-r8',
						'~/android-ndk-r7',
						'~/android-ndk']
			for c in candidates:
				fullPath = os.path.abspath( os.path.expanduser(c) )
				if os.path.isdir(fullPath):
					ctx.env.ANDROIDNDK = fullPath
					break
		if ctx.env.ANDROIDNDK == []:
			ctx.fatal('Android NDK not found. Try using the --android_ndk option.')
		else:
			ctx.msg( 'Android NDK : ', ctx.env.ANDROIDNDK )


#----------------------------------------------------------------------------------------------
# Android: Build the native part of a project
#----------------------------------------------------------------------------------------------
def android_build_native( bld, path, ndkModulePath ):

	class android_build_native_task(Task):

		projectPath = ''
		ndkPath=''
		ndkModulePath=''

		def run(self):
			failed = False
			result = subprocess.call( 'NDK_MODULE_PATH='+ndkModulePath+' '+self.ndkPath+'/ndk-build'
										+' -C'+self.projectPath,
										shell=True)
			if result<0:
				failed = True
			return failed

	android_build_native_task = waflib.Task.always_run(android_build_native_task)
	bld.add_group()
	build_1 = android_build_native_task(env=bld.env)
	build_1.projectPath = path
	build_1.ndkPath = bld.env.ANDROIDNDK
	build_1.ndkModulePath = ndkModulePath
	build_1.color = 'GREEN'
	bld.add_to_group( build_1 )
	bld.add_group()


#----------------------------------------------------------------------------------------------
# Android: Build the java part of a project
#----------------------------------------------------------------------------------------------
def android_build( bld, path ):

	class android_build_task(Task):

		projectPath = ''
		sdkPath = ''

		def run(self):
			failed = False
			command = 'ant -Dsdk.dir='+self.sdkPath+' -f '+self.projectPath+'/build.xml debug'
			print command
			result = subprocess.call( command, shell=True)
			if result<0:
				failed = True
			return failed

	android_build_task = waflib.Task.always_run(android_build_task)
	bld.add_group()
	build_1 = android_build_task(env=bld.env)
	build_1.projectPath = path
	build_1.sdkPath = bld.env.ANDROIDSDK
	build_1.color = 'GREEN'
	bld.add_to_group( build_1 )
	bld.add_group()


#----------------------------------------------------------------------------------------------
# Android: Build the java part of a project
#----------------------------------------------------------------------------------------------
def android_test( bld, path ):

	class android_test_task(Task):

		projectPath = ''
		sdkPath = ''

		def run(self):
			failed = False
			command = 'ant -Dsdk.dir='+self.sdkPath+' -f '+self.projectPath+'/build.xml debug install'
			print command
			result = subprocess.call( command, shell=True)
			if result<0:
				failed = True
			return failed

	android_test_task = waflib.Task.always_run(android_test_task)
	bld.add_group()
	build_1 = android_test_task(env=bld.env)
	build_1.projectPath = path
	build_1.sdkPath = bld.env.ANDROIDSDK
	build_1.color = 'GREEN'
	bld.add_to_group( build_1 )
	bld.add_group()


#--------------------------------------------------------------------------------------------------
# Android: Copy the crosa system resources in the android assets folder and generate an index file
#--------------------------------------------------------------------------------------------------
def android_copy_crosa_resources( bld, path, depot='Data/System', buildIndex=True ):

	class android_copy_crosa_resources_task(Task):

		projectPath = ''
		systemDataPath = ''
		buildIndex = True

		def run(self):

			if not os.path.exists( self.projectPath+'/assets' ):
				os.makedirs( self.projectPath+'/assets' )

			index = ''
			for f in glob.glob( self.systemDataPath+'/*.crosa' ):
				index += os.path.basename(f)+'\n'
				shutil.copy( os.path.abspath(f), self.projectPath+'/assets' )

			if buildIndex:
				file = open(self.projectPath+'/assets/crosa.index', 'w')
				file.writelines([index])

			return False

	android_copy_crosa_resources_task = waflib.Task.always_run(android_copy_crosa_resources_task)

	build_1 = android_copy_crosa_resources_task(env=bld.env)
	build_1.projectPath = path
	build_1.systemDataPath = bld.env.CROSA_ROOT+'/'+depot
	build_1.color = 'CYAN'
	build_1.buildIndex = buildIndex
	bld.add_to_group( build_1 )



#--------------------------------------------------------------------------------------------------
# Android: Copy resources directly to the sdcard of the default android device
#--------------------------------------------------------------------------------------------------
def android_copy_external_resources( ctx, sourceGlob, dest ):

	for f in glob.glob( sourceGlob ):
		ctx.cmd_and_log(
				[ ctx.env.ANDROIDSDK+'/platform-tools/adb', 'push', os.path.abspath(f), dest+'/'+os.path.basename(f) ],
				output=waflib.Context.STDOUT,
				quiet=None )


