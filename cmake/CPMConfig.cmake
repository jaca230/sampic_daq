# --------------------------------------------------------------------------
# List of CPM packages in dependency order
# --------------------------------------------------------------------------
set(CPM_PACKAGE_LIST
  nlohmann_json
  reflect_cpp
  spdlog
)

# ---------------------- nlohmann_json (header-only) ----------------------
set(nlohmann_json_REPO "nlohmann/json")
set(nlohmann_json_TAG "v3.11.2")
set(nlohmann_json_TARGET "")  # header-only
set(nlohmann_json_DOWNLOAD_ONLY YES)

# ---------------------- reflect-cpp (header-only) ----------------------
set(reflect_cpp_REPO "getml/reflect-cpp")
set(reflect_cpp_TAG "v0.21.0")
set(reflect_cpp_TARGET "reflectcpp")

# ---------------------- spdlog (header-only) ----------------------
set(spdlog_REPO "gabime/spdlog")
set(spdlog_TAG "v1.14.1")
set(spdlog_TARGET "")  # header-only
set(spdlog_DOWNLOAD_ONLY YES)
