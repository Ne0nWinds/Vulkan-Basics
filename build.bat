@echo off

clang -g main.c -o build\a.exe -IC:\VulkanSDK\1.2.162.1\Include\vulkan -lUser32.lib -lC:\VulkanSDK\1.2.162.1\Lib\vulkan-1.lib
