srpm:
	dnf -y install git rpm-build dnf-plugins-core libldb-devel
	export PRERELEASE=1
	./contrib/fedora/make_srpm.sh --output $(outdir)
