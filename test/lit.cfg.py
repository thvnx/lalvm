import lit.formats
import os

config.name = 'lalvm'
config.test_format = lit.formats.ShTest(True)
config.suffixes = ['.adb', '.mlir']
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = config.lalvm_obj_root

config.substitutions.append(('%lalvm',
    os.path.join(config.lalvm_tools_dir, 'lalvm')))
config.substitutions.append(('%FileCheck',
    os.path.join(config.llvm_tools_dir, 'FileCheck')))
config.substitutions.append(('%not',
    os.path.join(config.llvm_tools_dir, 'not')))
