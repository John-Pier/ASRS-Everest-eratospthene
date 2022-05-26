#define _CRT_SECURE_NO_DEPRECATE

const int NUMBER_OF_MEDIATORS = 6;
const char* APP_ID = "628e8cb61300001000f9e774";
const char* TOKEN = "67lmsc3r1b6ga82b1tf2mkrnoyu2sk2681tmebaw2gaihppk0dwflgycial1xppc";

#pragma cling load("libcurl")

#include <templet.hpp>
#include <everest.hpp>
#include <cmath>
#include <iostream>
#include <ctime>

class ActorMessage : public templet::message {
public:
    ActorMessage(templet::actor *a = 0, templet::message_adaptor ma = 0) : templet::message(a, ma) {
    }
    int current_value;
};

#pragma templet !SourceActor(out!ActorMessage)

struct SourceActor : public templet::actor {
    static void on_out_adapter(templet::actor *a, templet::message *m) {
        ((SourceActor *) a)->on_out(*(ActorMessage *) m);
    }

    SourceActor(templet::engine &e) : SourceActor() {
        SourceActor::engines(e);
    }

    SourceActor() : templet::actor(true), out(this, &on_out_adapter) {
    }

    void engines(templet::engine &e) {
        templet::actor::engine(e);
    }

    void start() {
        on_out(out);
    }

    inline void on_out(ActorMessage &m) {
        out.current_value = next_value++;
        out.send();
        std::cout << "Source send ActorMessage: " << out.current_value << std::endl;
    }

    ActorMessage out;
    long next_value = 2;
};

#pragma templet MediatorActor(in?ActorMessage, out!ActorMessage, t:everest)

struct MediatorActor : public templet::actor {
    static void on_in_adapter(templet::actor *a, templet::message *m) {
        ((MediatorActor *) a)->on_in(*(ActorMessage *) m);
    }

    static void on_out_adapter(templet::actor *a, templet::message *m) {
        ((MediatorActor *) a)->on_out(*(ActorMessage *) m);
    }

    static void on_t_adapter(templet::actor *a, templet::task *t) {
        ((MediatorActor *) a)->on_t(*(templet::everest_task *) t);
    }

    MediatorActor(templet::engine &e, templet::everest_engine &te_everest) : MediatorActor() {
        MediatorActor::engines(e, te_everest);
    }

    MediatorActor() : templet::actor(false),
                      out(this, &on_out_adapter),
                      t(this, &on_t_adapter) {
        _in = 0;
        prime_value = 0;
        t.app_id(APP_ID);
    }

    void engines(templet::engine &e, templet::everest_engine &te_everest) {
        templet::actor::engine(e);
        t.engine(te_everest);
    }

    inline void on_in(ActorMessage &m) {
        _in = &m;
        if (access(_in) && access(out)) {
            current_value_in_actor = _in->current_value;
            std::cout << "on_in Actor: " << current_value_in_actor << "; Current prime_value: " << prime_value << std::endl;

            if (prime_value) {
                json inJson;
                inJson["name"] = "is-prime-app";
                inJson["inputs"]["checked"] = current_value_in_actor;
                inJson["inputs"]["prime"] = prime_value;

                if (t.submit(inJson)) {
                    std::cout << "Task: value send in Everest app" << std::endl;
                }
            } else {
                prime_value = current_value_in_actor;
                std::cout << mediator_ID << ": Set prime in MediatorActor: " << prime_value << std::endl;
                _in->send();
            }
        }
    }

    inline void on_out(ActorMessage &m) {
        if (access(_in) && access(out)) {
            current_value_in_actor = _in->current_value;
            std::cout << "on_out (chain) ActorMessage for value: " << current_value_in_actor << std::endl;
            _in->send();
        }
    }

    inline void on_t(templet::everest_task &t) {
        std::cout << "on_t Actor: Task completed, current prime: " << prime_value << std::endl;

        json resultJson = t.result();

        if (prime_value) {
            int result = resultJson["result"];
            if (result != 0) {
                std::cout << mediator_ID <<": Value is prime for current, send next -> " << current_value_in_actor << std::endl;
                out.current_value = current_value_in_actor;
                out.send();
            } else {
                _in->send();
            }
        }
    }

    void in(ActorMessage &m) { m.bind(this, &on_in_adapter); }

    ActorMessage out;
    templet::everest_task t;
    ActorMessage *_in;
    long current_value_in_actor;
    int mediator_ID;
    long prime_value;
};

#pragma templet DestinationActor(in?ActorMessage)

struct DestinationActor : public templet::actor {
    static void on_in_adapter(templet::actor *a, templet::message *m) {
        ((DestinationActor *) a)->on_in(*(ActorMessage *) m);
    }

    DestinationActor(templet::engine &e) : DestinationActor() {
        DestinationActor::engines(e);
    }

    DestinationActor() : templet::actor(false) {
        last_prime_value = 0;
    }

    void engines(templet::engine &e) {
        templet::actor::engine(e);
    }

    inline void on_in(ActorMessage &m) {
        last_prime_value = m.current_value;
        std::cout << "Finish! Set prime in DestinationActor: " << last_prime_value << std::endl;
        stop();
    }

    void in(ActorMessage &m) { m.bind(this, &on_in_adapter); }

    long last_prime_value;
};

int main() {
    templet::engine eng;
    templet::everest_engine teng(TOKEN);

    if (!teng) {
        std::cout << "Task engine is not connected to the Everest server..." << std::endl;
        return EXIT_FAILURE;
    }

    SourceActor source_worker(eng);
    MediatorActor mediator_worker[NUMBER_OF_MEDIATORS];
    DestinationActor destination_worker(eng);

    mediator_worker[0].in(source_worker.out);

    for (int i = 1; i < NUMBER_OF_MEDIATORS; i++) {
        mediator_worker[i].in(mediator_worker[i - 1].out);
    }

    destination_worker.in(mediator_worker[NUMBER_OF_MEDIATORS - 1].out);

    source_worker.next_value = 2;

    for (int i = 0; i < NUMBER_OF_MEDIATORS; i++) {
        mediator_worker[i].mediator_ID = i + 1;
        mediator_worker[i].engines(eng, teng);
    }

    srand(time(NULL));

    eng.start();
    teng.run();

    if (eng.stopped()) {
        std::cout << "All primes are found" << std::endl;

        for (int i = 0; i < NUMBER_OF_MEDIATORS; i++) {
            std::cout << i + 1 << ": Prime - " << mediator_worker[i].prime_value << std::endl;
        }
        std::cout << NUMBER_OF_MEDIATORS + 1 << ": Prime - " << destination_worker.last_prime_value << std::endl;

        std::cout << "Enter some value to exit...." << std::endl;
        std::cin.get();
        return EXIT_SUCCESS;
    }

    templet::everest_error cntxt;

    teng.print_error(&cntxt);

    if (teng.error(&cntxt)) {
        std::cout << "...task engine error..." << std::endl;
        std::cout << "error information:" << std::endl;

        std::cout << "type ID : " << *cntxt._type << std::endl;
        std::cout << "HTML response code : " << cntxt._code << std::endl;
        std::cout << "HTML response : " << cntxt._response << std::endl;
        std::cout << "task input : " << cntxt._task_input << std::endl;


        switch (*cntxt._type) {
            case templet::everest_error::NOT_ERROR: {
                std::cout << "error string : NOT_ERROR" << std::endl;
            }
            case templet::everest_error::NOT_CONNECTED: {
                std::cout << "error string : NOT_CONNECTED" << std::endl;
                std::cout << "the task engine is not initialized properly -- fatal error, exiting" << std::endl;
                return EXIT_FAILURE;
            }
            case templet::everest_error::SUBMIT_FAILED: {
                std::cout << "error string : SUBMIT_FAILED" << std::endl;
                std::cout << "resubmitting the task" << std::endl;
                json input = json::parse(cntxt._task_input);
                cntxt._task->resubmit(input);
                break;
            }
            case templet::everest_error::TASK_CHECK_FAILED: {
                std::cout << "error string : TASK_CHECK_FAILED" << std::endl;
                std::cout << "trying to check the task status again" << std::endl;
                *cntxt._type = templet::everest_error::NOT_ERROR;
                break;
            }
            case templet::everest_error::TASK_FAILED_OR_CANCELLED: {
                std::cout << "error string : TASK_FAILED_OR_CANCELLED" << std::endl;
                std::cout << "resubmitting the task" << std::endl;
                json input = json::parse(cntxt._task_input);
                // here you may fix something in the input
                cntxt._task->resubmit(input);
            }
        }
    } else {
        std::cout << "Error! Logical error" << std::endl;
    }

    return EXIT_FAILURE;
}