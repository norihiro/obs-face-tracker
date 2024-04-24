FROM fedora:%releasever%

RUN dnf install -y rpm-build python3-dnf-plugins-core && dnf clean all
RUN dnf install -y obs-studio obs-studio-devel cmake gcc gcc-c++ && dnf clean all
RUN dnf install -y qt6-qtbase-devel qt6-qtbase-private-devel && dnf clean all

RUN useradd -s /bin/bash -m rpm
RUN echo >> /etc/sudoers
RUN echo "rpm ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER rpm
WORKDIR /home/rpm
