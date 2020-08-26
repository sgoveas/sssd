srpm:
	dnf -y install git rpm-build dnf-plugins-core libldb-devel
	./contrib/fedora/make_srpm.sh

	if [[ "${outdir}" != "" ]]; then \
		mv /builddir/build/SRPMS/* ${outdir}; \
	fi
