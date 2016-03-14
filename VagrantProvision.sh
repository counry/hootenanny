#!/usr/bin/env bash

echo "Updating OS..."
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get dist-upgrade -y
sudo apt-get install ntp -y

# Add the postgres repo
if [ ! -f /etc/apt/sources.list.d/pgdg.list ]; then
    echo "Adding PostgreSQL repository to apt..."
    sudo bash -c "echo 'deb http://apt.postgresql.org/pub/repos/apt/ '$(lsb_release -cs)'-pgdg main' > /etc/apt/sources.list.d/pgdg.list"
    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
    sudo apt-get update -y
    sudo apt-get upgrade -y
fi

echo "Installing dependencies from repos..."
sudo apt-get install -y texinfo g++ libicu-dev libqt4-dev git-core libboost-dev libcppunit-dev libcv-dev libopencv-dev libgdal-dev liblog4cxx10-dev libnewmat10-dev libproj-dev python-dev libjson-spirit-dev automake1.11 protobuf-compiler libprotobuf-dev gdb libqt4-sql-psql libgeos++-dev swig lcov tomcat6 openjdk-7-jdk openjdk-7-dbg maven libstxxl-dev nodejs-dev nodejs-legacy doxygen xsltproc asciidoc pgadmin3 curl npm libxerces-c28 libglpk-dev libboost-all-dev source-highlight texlive-lang-arabic texlive-lang-hebrew texlive-lang-cyrillic graphviz w3m python-setuptools python python-pip git ccache libogdi3.2-dev gnuplot python-matplotlib libqt4-sql-sqlite wamerican-insane

if ! dpkg -l | grep --quiet wamerican-insane; then
    # See /usr/share/doc/dictionaries-common/README.problems for details
    # http://www.linuxquestions.org/questions/debian-26/dpkg-error-processing-dictionaries-common-4175451951/
    sudo apt-get install -y wamerican-insane
    sudo /usr/share/debconf/fix_db.pl
    sudo dpkg-reconfigure dictionaries-common
fi

sudo apt-get autoremove -y

# cucumber deps install for ui tests

cd ~

sudo apt-get install ruby ruby-dev xvfb zlib1g-dev patch x11vnc unzip

if ! grep --quiet "export HOOT_HOME" ~/.profile; then
    echo "Adding hoot home to profile..."
    echo "export HOOT_HOME=/home/vagrant/hoot" >> ~/.profile
fi
if ! grep --quiet "\$HOME/.gem/ruby/1.9.1/bin:\$HOME/bin:\$HOOT_HOME/bin" ~/.profile; then
    echo "Adding path vars to profile..."
    echo "export PATH=\$PATH:\$HOME/.gem/ruby/1.9.1/bin:\$HOME/bin:\$HOOT_HOME/bin" >> ~/.profile
fi
source ~/.profile

gem list --local | grep -q selenium-cucumber
if [ $? -eq 1 ]; then
    echo "Installing selenium-cucumber gem..."
    sudo gem install --user-install selenium-cucumber
fi
gem list --local | grep -q mime-types
if [ $? -eq 1 ]; then
    echo "Installing mime-types gem..."
    sudo gem install --user-install mime-types -v 2.6.2
fi
gem list --local | grep -q capybara
if [ $? -eq 1 ]; then
    echo "Installing capybara gem..."
    sudo gem install --user-install capybara -v 2.5.0
fi
gem list --local | grep -q capybara-webkit
if [ $? -eq 1 ]; then
    echo "Installing capybara-webkit gem..."
    sudo gem install --user-install capybara-webkit
fi
gem list --local | grep -q rspec
if [ $? -eq 1 ]; then
    echo "Installing rspec gem..."
    sudo gem install --user-install rspec
fi

if [ ! -f google-chrome-stable_current_amd64.deb ]; then
    echo "Installing Chrome..."
    wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
    sudo dpkg -i google-chrome-stable_current_amd64.deb
    sudo apt-get -f install -y
fi

if [ ! -f bin/chromedriver ]; then
    echo "Installing Chromedriver..."
    mkdir -p $HOME/bin
    wget http://chromedriver.storage.googleapis.com/2.14/chromedriver_linux64.zip
    unzip -d $HOME/bin chromedriver_linux64.zip
fi

sudo apt-get autoremove -y

# end cucumber install

# Hoot Baseline is PostgreSQL 9.1 and PostGIS 1.5, so we need a deb file and
# then remove 9.5
if [ ! -f postgresql-9.1-postgis_1.5.3-2_amd64.deb ]; then
    echo "Installing PostgreSQL 9.1..."
    wget http://launchpadlibrarian.net/86690107/postgresql-9.1-postgis_1.5.3-2_amd64.deb
    sudo dpkg -i postgresql-9.1-postgis_1.5.3-2_amd64.deb
    sudo apt-get -f install -y
    # fixes missing dependency of postgis 1.5 by installing postgresql 9.1. 9.1 is installed listening on the default port, 5432. It unfortunately also installs postgres 9.5 but we remove that cleanly in the following steps, while leaving postgres 9.1 untouched
    echo "Removing PostgreSQL 9.5..."
    sudo apt-get purge -y postgresql-9.5 postgresql-client-9.5 postgresql-9.5-postgis-scripts
    sudo apt-get install -y postgresql-contrib-9.1
fi

# Get GDAL & FileGDB_API source
if [ ! -f gdal-1.10.1.tar.gz ]; then
    echo "Downloading GDAL source..."
    wget http://download.osgeo.org/gdal/1.10.1/gdal-1.10.1.tar.gz
fi
if [ ! -d gdal-1.10.1 ]; then
    echo "Extracting GDAL source..."
    tar zxvfp gdal-1.10.1.tar.gz
fi
if [ ! -f FileGDB_API_1_3-64.tar.gz ]; then
    echo "Downloading FileGDB API source..."
    wget http://downloads2.esri.com/Software/FileGDB_API_1_3-64.tar.gz
fi
if [ ! -d /usr/local/FileGDB_API ]; then
    echo "Extracting FileGDB API source & installing lib..."
    sudo tar xvfp FileGDB_API_1_3-64.tar.gz --directory /usr/local
    sudo sh -c "echo '/usr/local/FileGDB_API/lib' > /etc/ld.so.conf.d/filegdb.conf"
fi

# Build GDAL w/ FileGDB
if ! ogrinfo --formats | grep --quiet FileGDB; then
    echo "Building GDAL w/ FileGDB..."
    export PATH=/usr/local/lib:/usr/local/bin:$PATH
    cd gdal-1.10.1
    sudo ./configure --with-fgdb=/usr/local/FileGDB_API --with-pg=/usr/bin/pg_config --with-python
    sudo make -j$(nproc)
    sudo make install
    cd swig/python
    python setup.py build
    sudo python setup.py install
    sudo ldconfig
    cd ~
fi

if ! grep --quiet NODE_PATH ~/.profile; then
    echo "Installing node js dependencies..."
    sudo npm config set registry http://registry.npmjs.org/
    sudo npm install -g xml2js htmlparser imagemagick mocha@1.20.1 express@3.1.2 async html-to-text restler
    echo 'Adding NODE_PATH to user environment...'
    echo 'export NODE_PATH=/usr/local/lib/node_modules' >> ~/.profile
    source ~/.profile
fi

# Module needed for OSM API db test
if [ ! -d $HOME/.cpan ]; then
    echo "Installing XML::Simple module..."
    (echo y;echo o conf prerequisites_policy follow;echo o conf commit)|sudo cpan
    sudo perl -MCPAN -e 'install XML::Simple'
fi

# Create Services Database
if ! sudo -u postgres psql -lqt | grep -i --quiet hoot; then
    echo "Creating Services Database..."
    sudo -u postgres createuser --superuser hoot
    sudo -u postgres psql -c "alter user hoot with password 'hoottest';"
    sudo -u postgres createdb hoot --owner=hoot
    sudo -u postgres createdb wfsstoredb --owner=hoot
    sudo -u postgres psql -d hoot -c 'create extension hstore;'
    sudo -u postgres psql -d postgres -c "UPDATE pg_database SET datistemplate='true' WHERE datname='wfsstoredb'"
    sudo -u postgres psql -d wfsstoredb -f /usr/share/postgresql/9.1/contrib/postgis-1.5/postgis.sql
    sudo -u postgres psql -d wfsstoredb -f /usr/share/postgresql/9.1/contrib/postgis-1.5/spatial_ref_sys.sql
    sudo -u postgres psql -d wfsstoredb -c "GRANT ALL on geometry_columns TO PUBLIC;"
    sudo -u postgres psql -d wfsstoredb -c "GRANT ALL on geography_columns TO PUBLIC;"
    sudo -u postgres psql -d wfsstoredb -c "GRANT ALL on spatial_ref_sys TO PUBLIC;"
fi

# PostgreSQL Tuning
if ! grep -i --quiet HOOT /etc/postgresql/9.1/main/postgresql.conf; then
echo "Tuning PostgreSQL..."
sudo -u postgres sed -i.bak s/^max_connections/\#max_connections/ /etc/postgresql/9.1/main/postgresql.conf
sudo -u postgres sed -i.bak s/^shared_buffers/\#shared_buffers/ /etc/postgresql/9.1/main/postgresql.conf
sudo -u postgres bash -c "cat >> /etc/postgresql/9.1/main/postgresql.conf" <<EOT

#--------------
# Hoot Settings
#--------------
max_connections = 1000
shared_buffers = 1024MB
max_files_per_process = 1000
work_mem = 16MB
maintenance_work_mem = 256MB
checkpoint_segments = 20
autovacuum = off
EOT
fi

# Update shared memory limits in OS
if ! sysctl -e kernel.shmmax | grep --quiet 1173741824; then
    echo "Setting kernel.shmmax..."
    sudo sysctl -w kernel.shmmax=1173741824
    sudo sh -c "echo 'kernel.shmmax=1173741824' >> /etc/sysctl.conf"
fi
if ! sysctl -e kernel.shmall | grep --quiet 2097152; then
    echo "Setting kernel.shmall..."
    sudo sysctl -w kernel.shmall=2097152
    sudo sh -c "echo 'kernel.shmall=2097152' >> /etc/sysctl.conf"
fi

sudo service postgresql restart

# Configure Tomcat

if ! grep --quiet TOMCAT6_HOME ~/.profile; then
    echo "Adding Tomcat to profile..."
    echo "export TOMCAT6_HOME=/usr/share/tomcat6" >> ~/.profile
    source ~/.profile
    cd $TOMCAT6_HOME
    sudo ln -s /var/lib/tomcat6/webapps webapps
    sudo ln -s /var/lib/tomcat6/conf conf
    sudo ln -s /var/log/tomcat6 log
    cd ~
fi

# the validity of these chown's may be questionable; they're needed so that the ui tests 
# can deploy code to tomcat
sudo chown -R vagrant:tomcat6 $TOMCAT6_HOME
sudo mkdir -p /var/lib/tomcat6/logs
sudo mkdir -p $TOMCAT6_HOME/logs
sudo chown -R vagrant:tomcat6 $TOMCAT6_HOME/logs
sudo chown -R vagrant:tomcat6 /var/lib/tomcat6
sudo chown -R vagrant:tomcat6 /etc/tomcat6
#sudo chown -R vagrant:tomcat6 /etc/default/tomcat6
sudo chown -R tomcat6:tomcat6 /var/log/tomcat6

cd $HOOT_HOME
source ./SetupEnv.sh

# Check that hoot-ui submodule has been init'd and updated
if [ ! "$(ls -A hoot-ui)" ]; then
    echo "hoot-ui is empty"
    echo "init'ing and updating submodule"
    git submodule init && git submodule update
fi

if ! grep -i --quiet HOOT /etc/default/tomcat6; then
echo "Configuring tomcat6 environment..."
sudo bash -c "cat >> /etc/default/tomcat6" <<EOT

#--------------
# Hoot Settings
#--------------
HOOT_HOME=/home/vagrant/hoot
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:$HOOT_HOME/lib:$HOOT_HOME/pretty-pipes/lib
GDAL_DATA=/usr/local/share/gdal
GDAL_LIB_DIR=/usr/local/lib
HOOT_WORKING_NAME=hoot
PATH=$HOOT_HOME/bin:$PATH
EOT
fi

if ! grep -i --quiet 'umask 002' /etc/default/tomcat6; then
echo "Changing Tomcat umask to group write..."
sudo bash -c "cat >> /etc/default/tomcat6" <<EOT
# Set tomcat6 umask to group write because all files in shared folder are owned by vagrant
umask 002
EOT
fi

if grep -i --quiet '^JAVA_OPTS=.*\-Xmx128m' /etc/default/tomcat6; then
    echo "Changing Tomcat java opts..."
    sudo sed -i.bak "s@\-Xmx128m@\-Xms512m \-Xmx2048m \-XX:PermSize=512m \-XX:MaxPermSize=4096m@" /etc/default/tomcat6
fi

if grep -i --quiet 'gdal/1.10' /etc/default/tomcat6; then
    echo "Fixing Tomcat GDAL_DATA env var path..."
    sudo sed -i.bak s@^GDAL_DATA=.*@GDAL_DATA=\/usr\/local\/share\/gdal@ /etc/default/tomcat6
fi

# Remove gdal libs installed by libgdal-dev that interfere with
# renderdb-export-server using gdal libs compiled from source (fgdb support)
if [ -f "/usr/lib/libgdal.*" ]; then
    echo "Removing GDAL libs installed by libgdal-dev..."
    sudo rm /usr/lib/libgdal.*
fi

mkdir -p $HOOT_HOME/ingest/processed
chown -R vagrant:tomcat6 $HOOT_HOME/ingest
if ! grep -i --quiet 'ingest/processed' /etc/tomcat6/server.xml; then
    echo "Adding Tomcat context path for tile images..."
    sudo sed -i.bak "s@<\/Host>@  <Context docBase=\"\/home\/vagrant\/hoot\/ingest\/processed\" path=\"\/static\" \/>\n      &@" /etc/tomcat6/server.xml
fi

if ! grep -i --quiet 'allowLinking="true"' /etc/tomcat6/context.xml; then
    echo "Set allowLinking to true in Tomcat context..."
    sudo sed -i.bak "s@^<Context>@<Context allowLinking=\"true\">@" /etc/tomcat6/context.xml
fi

if [ ! -d $TOMCAT6_HOME/.deegree ]; then
    echo "Creating directory for webapp..."
    sudo mkdir $TOMCAT6_HOME/.deegree
    sudo chown vagrant:tomcat6 $TOMCAT6_HOME/.deegree
fi

# Update marker file date now that dependency and config stuff has run
# The make command will exit and provide a warning to run 'vagrant provision'
# if the marker file is older than this file (VagrantProvision.sh)
touch Vagrant.marker

# Hoot
echo "Configuring Hoot..."
echo HOOT_HOME: $HOOT_HOME
cp conf/DatabaseConfig.sh.orig conf/DatabaseConfig.sh

if [ ! -f /etc/init.d/node-mapnik-server ]; then
  echo "Installing node-mapnik-server..."
  sudo cp $HOOT_HOME/node-mapnik-server/init.d/node-mapnik-server /etc/init.d
  sudo chmod a+x /etc/init.d/node-mapnik-server
  # Make sure all npm modules are installed
  cd node-mapnik-server
  sudo npm install
  cd ..
fi

aclocal && autoconf && autoheader && automake && ./configure --with-rnd --with-services --with-uitests
if [ ! -f LocalConfig.pri ] && ! grep --quiet QMAKE_CXX LocalConfig.pri; then
    echo 'Customizing LocalConfig.pri...'
    cp LocalConfig.pri.orig LocalConfig.pri
    echo 'QMAKE_CXX=ccache g++' >> LocalConfig.pri
fi
echo "Building Hoot...  Will take several extra minutes to build the training data the initial time Hootenanny is installed only."
#make clean-all -sj$(nproc)
# The services build won't always complete the first time without errors (for some unknown reason caused by the Maven pom), so we're executing multiple compiles here to get around that.
make -sj$(nproc) &> /dev/null || true
make -s &> /dev/null || true
make -sj$(nproc)
scripts/DeployTomcat.sh
make -sj$(nproc) docs 
hoot version

echo "See VAGRANT.md for additional configuration instructions and then run 'vagrant ssh' to log into the Hootenanny virtual machine."
echo "See the 'docs' directory on the virtual machine for Hootenanny documentation files."
echo "Access the web application at http://localhost:8888/hootenanny-id"
echo "If you wish to run the diagnostic tests, log into the virtual machine and run: 'cd $HOOT_HOME && make -sj$(nproc) test-all'"


