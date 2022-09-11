#! /usr/bin/env sh

if [ "$#" -ne 2 ];
then
  printf "Usage: ./deploy.sh exe directory\n"
fi

list=$(ldd $1 | grep /mingw64 | sed 's/.dll.*/.dll/')
for dll in $list;
do
  pkg=`pacman -Qo $dll | sed 's/.* is owned by //' | tr ' ' '-'`
  pkglist="$pkglist $pkg"
done
# remove duplicates
pkglist=`echo $pkglist | tr ' ' '\n' | sort | uniq`
printf "$pkglist\n"

mkdir -p "$2"

for pkg in $pkglist
do
  tmp=`mktemp -d`
  cd $tmp
  if [ -f /var/cache/pacman/pkg/$pkg-any.pkg.tar.xz ]; then
    tar -xf /var/cache/pacman/pkg/$pkg-any.pkg.tar.xz
  fi
  if [ -f /var/cache/pacman/pkg/$pkg-any.pkg.tar.zst ]; then
    tar --use-compress-program=unzstd -xf /var/cache/pacman/pkg/$pkg-any.pkg.tar.zst
  fi
  # more fine-grained control is possible here
  cp -r $PWD/mingw64/bin $2
  cp -r $PWD/mingw64/share $2
done
