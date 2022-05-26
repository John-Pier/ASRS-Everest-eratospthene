/*$TET$$header*/
/*--------------------------------------------------------------------------*/
/*  Copyright 2021 Sergei Vostokin                                          */
/*                                                                          */
/*  Licensed under the Apache License, Version 2.0 (the "License");         */
/*  you may not use this file except in compliance with the License.        */
/*  You may obtain a copy of the License at                                 */
/*                                                                          */
/*  http://www.apache.org/licenses/LICENSE-2.0                              */
/*                                                                          */
/*  Unless required by applicable law or agreed to in writing, software     */
/*  distributed under the License is distributed on an "AS IS" BASIS,       */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/*  See the License for the specific language governing permissions and     */
/*  limitations under the License.                                          */
/*--------------------------------------------------------------------------*/

#define _CRT_SECURE_NO_DEPRECATE

const int NUMBER_OF_MEDIATORS = 6;
const int NUMBER_OF_BRICKS = 1;
const int NUMBER_OF_MAX_BRICKS = 100;

#pragma cling load("libcurl")

#include <templet.hpp>
#include <everest.hpp>
#include <cmath>
#include <iostream>
#include <ctime>

class brick : public templet::message {
public:
    brick(templet::actor *a = 0, templet::message_adaptor ma = 0) : templet::message(a, ma) {}

    int brick_ID;
};

#pragma templet !source(out!brick)

struct source : public templet::actor {
    static void on_out_adapter(templet::actor *a, templet::message *m) {
        ((source *) a)->on_out(*(brick *) m);
    }

    source(templet::engine &e) : source() {
        source::engines(e);
    }

    source() : templet::actor(true), out(this, &on_out_adapter) {
    }

    void engines(templet::engine &e) {
        templet::actor::engine(e);
    }

    void start() {
        on_out(out);
    }

    inline void on_out(brick &m) {
        if (number_of_bricks < NUMBER_OF_MAX_BRICKS) {
            out.brick_ID = number_of_bricks++;
            std::cout << "Source send brick: " << out.brick_ID << std::endl;
            out.send();
        } else {
            std::cout << "Source stop " << number_of_bricks << std::endl;
            stop();
        }
    }

    brick out;
    int number_of_bricks = 2;
};

#pragma templet mediator(in?brick, out!brick, t:everest)

struct mediator : public templet::actor {
    static void on_in_adapter(templet::actor *a, templet::message *m) {
        ((mediator *) a)->on_in(*(brick *) m);
    }

    static void on_out_adapter(templet::actor *a, templet::message *m) {
        ((mediator *) a)->on_out(*(brick *) m);
    }

    static void on_t_adapter(templet::actor *a, templet::task *t) {
        ((mediator *) a)->on_t(*(templet::everest_task *) t);
    }

    mediator(templet::engine &e, templet::everest_engine &te_everest) : mediator() {
        mediator::engines(e, te_everest);
    }

    mediator() : templet::actor(false),
                 out(this, &on_out_adapter),
                 t(this, &on_t_adapter) {
        _in = 0;
        prime_value = 0;

        t.app_id("628e8cb61300001000f9e774");
    }

    void engines(templet::engine &e, templet::everest_engine &te_everest) {
        templet::actor::engine(e);
        t.engine(te_everest);
    }

    inline void on_in(brick &m) {
        _in = &m;
        if (access(_in) && access(out)) {
            brick_ID = _in->brick_ID;
            std::cout << "on_in brick: " << brick_ID << "; Current prime: "<< prime_value << std::endl;

            if (prime_value) {
                inJson["name"] = "is-prime-app";
                inJson["inputs"]["checked"] = brick_ID;
                inJson["inputs"]["prime"] = prime_value;
                if (t.submit(inJson)) {
                    std::cout << "task submit succeeded" << std::endl;
//                _in->send();
                } else {
                    std::cout << "task submit failed" << std::endl;
                }
            } else {
                prime_value = brick_ID;
                std::cout << mediator_ID << ": " << "set prime in mediator: " << prime_value << std::endl;
                _in->send();
            }
        }
    }

    inline void on_out(brick &m) {
        if (access(_in) && access(out)) {
            brick_ID = _in->brick_ID;
            std::cout << "on_out take_a_brick: " << brick_ID << std::endl;
            _in->send();

//            t.submit(inJson);
        }
    }

    inline void on_t(templet::everest_task &t) {
        std::cout << "Task comlete; Brick: " << brick_ID << "; Current prime: "<< prime_value << std::endl;

        json resultJson = t.result();
        std::cout << "on_t resultJson: " << resultJson << std::endl;

        if (prime_value) {
            int result = resultJson["result"];
            if (result != 0) {
                std::cout << "Value is prime for current, send next: " << brick_ID << std::endl;
                out.brick_ID = brick_ID;
                out.send();
            } else {
                _in->send();
            }
//            if (brick_ID % prime_value == 0) {
//                return;
//            }

        } else {
//            prime_value = brick_ID;
//            std::cout << mediator_ID << ": " << "set prime in mediator: " << prime_value << std::endl;
//            _in->send();
        }

//        _in->send();
    }

    void in(brick &m) { m.bind(this, &on_in_adapter); }

    brick out;
    templet::everest_task t;
    brick *_in;
    json inJson;
    int brick_ID;
    int mediator_ID;
    int prime_value;
};

#pragma templet destination(in?brick)

struct destination : public templet::actor {
    static void on_in_adapter(templet::actor *a, templet::message *m) {
        ((destination *) a)->on_in(*(brick *) m);
    }

    destination(templet::engine &e) : destination() {
        destination::engines(e);
    }

    destination() : templet::actor(false) {
    }

    void engines(templet::engine &e) {
        templet::actor::engine(e);
    }

    inline void on_in(brick &m) {
        std::cout << "Finish! set prime in destination: " << m.brick_ID << std::endl;
        stop();
    }

    void in(brick &m) { m.bind(this, &on_in_adapter); }
};

int main() {
    templet::engine eng;
    templet::everest_engine teng("67lmsc3r1b6ga82b1tf2mkrnoyu2sk2681tmebaw2gaihppk0dwflgycial1xppc");
    // app id = 628e8cb61300001000f9e774

    if (!teng) {
        std::cout << "task engine is not connected to the Everest server..." << std::endl;
        return EXIT_FAILURE;
    }

    source source_worker(eng);
    mediator mediator_worker[NUMBER_OF_MEDIATORS];
    destination destination_worker(eng);

    mediator_worker[0].in(source_worker.out);

    for (int i = 1; i < NUMBER_OF_MEDIATORS; i++) {
        mediator_worker[i].in(mediator_worker[i - 1].out);
    }

    destination_worker.in(mediator_worker[NUMBER_OF_MEDIATORS - 1].out);

//    source_worker.number_of_bricks = NUMBER_OF_BRICKS;

    for (int i = 0; i < NUMBER_OF_MEDIATORS; i++) {
        mediator_worker[i].mediator_ID = i + 1;
        mediator_worker[i].engines(eng, teng);
    }

    srand(time(NULL));

    eng.start();
    teng.run();

    if (eng.stopped()) {
        std::cout << "All primes are found" << std::endl;
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
                // here you may fix something in the input
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
        std::cout << "logical error" << std::endl;
    }

    std::cout << "Error! Something broke..." << std::endl;
    return EXIT_FAILURE;
}