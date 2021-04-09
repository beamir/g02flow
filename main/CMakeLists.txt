set(COMPONENT_SRCS "timeTasks.c" "wifiTasks.c" "main.c" "pinTasks.c" "timeTasks.c" "nvsTasks.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_EMBED_TXTFILES
    "files/Gindex.html"
    "files/sample.txt"
)

set(COMPONENT_EMBED_FILES
    "files/pinout.jpg"
)

register_component()