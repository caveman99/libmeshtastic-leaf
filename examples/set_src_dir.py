import os
Import("env")

# Set the source directory to the example folder matching the environment name
example_name = env["PIOENV"]
src_dir = os.path.join(env.subst("$PROJECT_DIR"), example_name)

env.Replace(SRC_DIR=src_dir)
env["PROJECT_SRC_DIR"] = src_dir
env["PROJECTSRC_DIR"] = src_dir
