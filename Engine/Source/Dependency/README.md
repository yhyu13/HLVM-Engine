# How to upgrade VCPKG

1. Since we are using a fork of vcpkg, we need to update the fork first.
   Update the `latest` branch https://github.com/yhyu13/vcpkg/tree/latest to latest microsoft/vcpkg.
2. Backup the current vcpkg directory by `cp -r vcpkg vcpkg_old`
2. Make sure we are at latest `hlvm` branch and `git checkout -b hlvm_upgrade`. Then run `git pull --rebase origin latest`
   to rebase the `hlvm_upgrade` branch.
3. Make sure we run the all `Clean` scripts to remove `Build` directory and rebuild all targets
4. If vcpkg error occurs, we can always fall back to `hlvm` branch