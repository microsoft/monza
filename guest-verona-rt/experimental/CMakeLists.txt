# Interface library to enable epxerimental features in apps
# Add in addition to monza_app to your target
add_library(monza_experimental INTERFACE)
target_include_directories(monza_experimental INTERFACE include)
