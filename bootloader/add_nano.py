Import("env")


env.Append(
    LINKFLAGS=[ "-Wl,--wrap", "-Wl,__libc_init_array", "-Wl,--wrap", "-Wl,atexit" ] 
)
