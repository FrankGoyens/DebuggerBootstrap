from setuptools import setup, Extension
from Cython.Build import cythonize

setup(
    include_dirs=["../../server/protocol"],
    ext_modules =  cythonize(Extension("native_protocol", ["protocol.pyx", "../../server/protocol/Protocol.c"]))
)