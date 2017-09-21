/**
        * Copyright (c) 2017 LIBBLE team supervised by Dr. Wu-Jun LI at Nanjing University.
        * All Rights Reserved.
        * Licensed under the Apache License, Version 2.0 (the "License");
        * you may not use this file except in compliance with the License.
        * You may obtain a copy of the License at
        *
        * http://www.apache.org/licenses/LICENSE-2.0
        *
        * Unless required by applicable law or agreed to in writing, software
        * distributed under the License is distributed on an "AS IS" BASIS,
        * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
        * See the License for the specific language governing permissions and
        * limitations under the License. */

#ifndef _SERVER_HPP_
#define _SERVER_HPP_

#include <thread>
#include "../storage/include_storage.hpp"
#include "../util/include_util.hpp"
#include "Trainer.hpp"

class Server : public Trainer {
   private:
    char info;
    int server_id;
    Parameter params;
    Gradient_Dense grad;
    double rate;
    double lambda;
    int param_init;

   public:
    Server(int n_ser, int n_wor, int n_c, int n_r, int n_e, int n_i, int mode_, std::string f,
           Model *model_p, Comm *comm_p, int proc_id, double lambda_, double r, int param_i)
        : Trainer(n_ser, n_wor, n_c, n_r, n_e, n_i, mode_, f, model_p, comm_p),
          server_id(proc_id),
          lambda(lambda_),
          rate(r),
          param_init(param_i) {
        int s = get_local_params_size(num_cols, num_servers, server_id);
        params.resize(s);
        grad.resize(s);

        // paramter initialization
        if (param_init == 0)
            params.reset();
        else
            params.parameter_random_init();
    }

    void work() override {
        push();
        // check if a exceed
        if (server_id == 1) {
            double check_a = 1;
            for (int i; i < num_epoches * (num_of_all_data / num_workers + 1); i++) {
                check_a *= (1 - rate * lambda);
            }
            std::cout << "check_a info(if its index exceed 300, over), a:" << check_a
                      << ", 1/a:" << 1 / check_a << std::endl;
        }

        for (int i = 0; i < num_iters; i++) {
            MPI_Barrier(MPI_COMM_WORLD);  // start
            push();

            scope_pull_part_full_grad();

            scope_push_full_grad();

            scope_pull_params();
            MPI_Barrier(MPI_COMM_WORLD);  // end
            push();
        }

        send_params_to_coordinator();
        // std::cout << "server " << server_id << " done" << std::endl;
    }

    void push() { comm_ptr->S_send_params_to_all_W(params); }

    void scope_push_full_grad() { comm_ptr->S_send_grads_to_all_W(grad); }

    void scope_pull_part_full_grad() {
        comm_ptr->S_recv_grads_from_all_W(grad);
        vector_divi(grad.gradient, num_of_all_data);
    }

    void scope_pull_params() {
        comm_ptr->S_recv_params_from_all_W(params);
        double a = 1, b = 0;
        for (int i = 0; i < num_of_all_data / num_workers; i++) {
            a = (1 - lambda * rate) * a;
            b = (1 - lambda * rate) * b - rate;
        }
        vector_multi_add(params.parameter, a / num_workers, grad.gradient, b);
    }

    // send parameters to coordinator for saving
    void send_params_to_coordinator() { comm_ptr->S_send_params_to_C(params); }
};

#endif