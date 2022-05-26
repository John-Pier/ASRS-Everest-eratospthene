FROM python:2.7

WORKDIR /usr/src/app

COPY get-pip.py get-pip.py
RUN python2 get-pip.py
RUN python2 -m pip install tornado==4.5.3
RUN git clone https://gitlab.com/everest/agent.git ./everest_agent
WORKDIR /usr/src/app/everest_agent
COPY config.json ./agent.conf
COPY config.json /etc/everest_agent/agent.conf
COPY config.json /etc/everest_agent/conf/agent.conf

#bin/start.sh -c agent.conf