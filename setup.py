from distutils.core import setup

setup(name='zperfmq',
          description="""ZeroMQ Performance Measurements""",
          version='0.0.0',
          url='https://github.com/brettviren/zperfmq',
          packages=['zperfmq'],
          package_dir={'': 'bindings/python'},
)
