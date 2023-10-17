/*
  © 2016 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.


  Simple instrumentation tool that just counts how instructions that are
  executed on the connected processors.
*/

#include <simics/simulator/conf-object.h>
#include <simics/simulator-iface/instrumentation-tool.h>
#include <simics/model-iface/cpu-instrumentation.h>
#include <simics/device-api.h>
#include <simics/model-iface/processor-info.h>
#include <simics/simulator-api.h>
#define MAX_INST 100
#define MAX_STATIONS 10
#define TYPE_LOAD 0
#define TYPE_SUM 1
#define LOAD_TIMEOUT 3
#define SUM_TIMEOUT 3
#define REG_IN_PROGRAM 3
#define REG_DEPENDENCY true

conf_class_t *connection_class;

/*This structure is used to create entries inside list of fetch instructions*/
typedef struct {
    logical_address_t address;
    char * disassembled_text;
    int state;
    int order;
} instruction_entry;

/*This structure is the base for the functional units/stations
 * it requires a current_timeout to be decremented in each cycle if it's in_use
 * You can add other variables here but don't remove the existing ones*/
typedef struct {
    int current_timeout;
    instruction_entry * curr_inst;
    bool in_use;
    //you add more
    int type;
} unit_functional_station;

typedef struct {
    char * name;
    bool in_use;
}reg_t;

/* The connection (between tool and provider) 
 * This structure is the "Connection". In essense the structure for the state machine of tomasulo
 * You can add more things to this struct. Optimally you don't need to change the existing ones.
 * */
typedef struct {
        conf_object_t obj;
        conf_object_t *provider;
        const processor_info_v2_interface_t * pi_iface;
        const cpu_instrumentation_subscribe_interface_t *cis_iface;
        const cpu_cached_instruction_interface_t *cci_iface;
        const cpu_instruction_query_interface_t *iq_iface;
        const cpu_instruction_decoder_interface_t *ir_iface;
        bool restore_rip;
        logical_address_t restore_rip_address;
        bool finish;
        logical_address_t finished_address;
        bool stall;
        logical_address_t stall_address;
        bool skip_instruction;
        instruction_entry fetch_instructions[MAX_INST];
        int fetch_instruction_size;
        instruction_entry * finished_instructions[MAX_INST];
        int finished_instructions_size;
        unit_functional_station * stations[MAX_STATIONS];
        int total_reservation_stations;
        int cycles;
        reg_t register_list[REG_IN_PROGRAM];
} connection_t;

/* The Simics instrumentation tool
 * The tool is just a parent structure that holds all the connection. 
 * In this example, we just have one connection to one cpu that will operate on the tomasulo algorithm
 * But in theory we could have multiple cores and hence a connection for each CPU
 * */
typedef struct {
        conf_object_t obj;
        int next_connection_number;
        int unit_sum_cycles;
        VECT(connection_t *) connections;        
} tomasulo_t;

/*
 * conf_object_t is a structure every Simics object inherits from. 
 * This is a helper function to return a pointer to the tomasulo_t 
 * This way we can operate directly over the properties of that struct instead of doing this every time in the code using a cast
 * */
FORCE_INLINE tomasulo_t *
tool_of_obj(conf_object_t *obj) { return (tomasulo_t *)obj; }

/*
 * conf_object_t is a structure every Simics object inherits from. 
 * This is a helper function to return a pointer to the connection_t
 * This way we can operate directly over the properties of that struct instead of doing this every time in the code using a cast
 * */
FORCE_INLINE connection_t *
conn_of_obj(conf_object_t *obj) { return (connection_t *)obj; }

/* Static pointer to the event that will be recorder by Simics and run every cycle.
 * You don't need to change this.
 * */

/*************************** Connection Object ********************************/

// When a new connection happens, we allocate the struct to record it
static conf_object_t *
ic_alloc(void *arg)
{
        connection_t *conn = MM_ZALLOC(1, connection_t);
        return &conn->obj;
}

// Used to delete any user created data. Not relevant to the excercise
static void
ic_pre_delete_connection(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->remove_connection_callbacks(conn->provider, obj);
}

// Deletes the connection object when no longer needed
static int
ic_delete_connection(conf_object_t *obj)
{
        MM_FREE(obj);
        return 0;
}

// Enables or disables the instrumentation
static void
ic_enable(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->enable_connection_callbacks(conn->provider, obj);
}

// Enables or disables the instrumentation
static void
ic_disable(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->disable_connection_callbacks(conn->provider, obj);

}

reg_t * get_instruction_register(conf_object_t * obj, char * instruction_reg ){
    connection_t *conn = conn_of_obj(obj);
    for (int i = 0; i < REG_IN_PROGRAM; ++i){        
        if(conn->register_list[i].name != NULL){
            if (strncmp(instruction_reg, conn->register_list[i].name, 2) == 0){
                return &conn->register_list[i];
            }
        } else{
            if (strncmp(instruction_reg,"0x",2)==0){
                conn->register_list[i].name = "0x";
                conn->register_list[i].in_use = false;
                return &conn->register_list[i];
            }
            conn->register_list[i].name = instruction_reg;
            return &conn->register_list[i];
        }
    }
} 

/*
 * Given a logical address as an input, it returns the instruction details at that logical address
 * that is part of the list of already fetch instructions
 * */
instruction_entry * get_instruction_details(conf_object_t * obj, logical_address_t address){
    connection_t *conn = conn_of_obj(obj);
    for (int i = 0; i < MAX_INST; ++i){
        if(conn->fetch_instructions[i].address == address){
            return &conn->fetch_instructions[i];
        }    
    }   
    return NULL;
}

bool already_processed(conf_object_t * obj, logical_address_t address){
    connection_t *conn = conn_of_obj(obj);
    bool already_processed = false;
    for (int i = 0; i < conn->finished_instructions_size; ++i){
        if(conn->finished_instructions[i]->address == address){
            already_processed = true;
            break;
        }
    }
    if(!already_processed){
        for(int i = 0; i < conn->total_reservation_stations; ++i){
            if( conn->stations[i]->in_use && conn->stations[i]->curr_inst->address == address){
                already_processed = true;
            }
        } 
    }
    return already_processed;
}

//Add to station with +1

/*
 *This function identifies the instruction and also the operands.
 *For the current address. This is the MOST important function you need to modify in this exercise. 
 *This function doesn't return anything but it's expected to change the state machine of tomasulo's algorithm
 *Specially, the following properties of the connection_t object to control the execution flow:
 *conn->restore_rip
 *
 * */

void identify_instruction_and_operand(conf_object_t * obj, conf_object_t * cpu, logical_address_t address){ 
    connection_t *conn = conn_of_obj(obj);

    /*Your logic starts here!*/
    //Finish an instruction in a station
    instruction_entry * entry = get_instruction_details(obj, address);
    for(int i = 0; i < conn->total_reservation_stations; ++i){ 
        if( conn->stations[i]->in_use && conn->stations[i]->current_timeout == 0){
            conn->restore_rip_address = address; //this is the address to restore after the station finishes the instruction
            conn->restore_rip = true; //controls that rip should be returned to the real address after finished instruction
            conn->finished_address = conn->stations[i]->curr_inst->address; //Address of the finished instruction
            conn->finish = true; //tells the state machine to finish an instruction in this cycle at finished address
            conn->stations[i]->in_use = false; //since the instruction is finised, this engine is no longer busy
            conn->stall = false; //just in case we were stalling before
            SIM_LOG_INFO(2, obj, 0, "I'm going to finishg here: %x and return to: %x\n, index %i",  conn->finished_address , conn->restore_rip_address , i);
            break;
        }  
    }
    //If I'm on the instruction that's about to finish, then we need to prioritize finish to avoid a loop
    //Add instruction to the station and DON'T finish it
    //you have 2 choices here, either you process the string or use the opcodes and identify the instruction
    //the operands are most likely easier to detect as a string
    bool available_station_load = false;
    bool available_station_sum = false;
    if(strncmp(entry->disassembled_text, "mov", 3) == 0) {
        for(int i = 0; i < conn->total_reservation_stations; ++i){ //Add instruction to station
            if(!conn->stations[i]->in_use && conn->stations[i]->type == TYPE_LOAD){
               conn->stations[i]->curr_inst = entry;
               conn->stations[i]->in_use = true;
               conn->stations[i]->current_timeout = LOAD_TIMEOUT; //we set the TIMEOUT to restart the instruction that just entered our station
               available_station_load = true; 
               conn->skip_instruction = true; //is already in a station, do not execute yet
            }
        }
    }
    if(strncmp(entry->disassembled_text, "add", 3) == 0) {
        for(int i = 0; i < conn->total_reservation_stations; ++i){
            if(!conn->stations[i]->in_use && conn->stations[i]->type == TYPE_SUM){
                conn->stations[i]->curr_inst = entry;
                conn->stations[i]->in_use = true;
                conn->stations[i]->current_timeout = SUM_TIMEOUT;
                available_station_sum = true;
                conn->skip_instruction = true; //is already in a station, do not execute yet
            }
        }
    }

    //No station can take this instruction that was fetch, we need to stall until they free up
    if(!available_station_sum && !available_station_load && !conn->finish){
        SIM_LOG_INFO(1, obj, 0, "I need to stall");
        conn->stall = true;
        conn->stall_address = address; //we'll loop in the current RIP until some engine finished and removes the stall
        return;
    }
    /*Here you can add all the other stations that you want, they can follow the usage of the same APIs*/
}

void identify_instruction_and_reg(conf_object_t * obj, conf_object_t * cpu, logical_address_t address){ 
    connection_t *conn = conn_of_obj(obj);

    /*Your logic starts here!*/
    //Finish an instruction in a station
    instruction_entry * entry = get_instruction_details(obj, address);
    get_instruction_register(obj, &entry->disassembled_text[4]);
    reg_t * reg1 = get_instruction_register(obj, &entry->disassembled_text[4]);
    reg_t * reg2 = get_instruction_register(obj, &entry->disassembled_text[8]);

    for(int i = 0; i < conn->total_reservation_stations; ++i){ 
        if(conn->stations[i]->in_use && conn->stations[i]->current_timeout == 0){
            for(int j = 0; j < REG_IN_PROGRAM; ++j){
                if(conn->register_list[j].name != NULL){
                    if(strncmp(&conn->stations[i]->curr_inst->disassembled_text[4], conn->register_list[j].name, 3) == 0){
                        conn->register_list[j].in_use = false;
                    }
                }
            }
            for(int k = 0; k < REG_IN_PROGRAM; ++k){
                if(conn->register_list[k].name != NULL){
                    if(strncmp(&conn->stations[i]->curr_inst->disassembled_text[8], conn->register_list[k].name, 2) == 0){
                        conn->register_list[k].in_use = false;
                    }
                }
            }
            conn->restore_rip_address = address; //this is the address to restore after the station finishes the instruction
            conn->restore_rip = true; //controls that rip should be returned to the real address after finished instruction
            conn->finished_address = conn->stations[i]->curr_inst->address; //Address of the finished instruction
            conn->finish = true; //tells the state machine to finish an instruction in this cycle at finished address
            conn->stations[i]->in_use = false; //since the instruction is finised, this engine is no longer busy
            conn->stall = false; //just in case we were stalling before
            SIM_LOG_INFO(2, obj, 0, "I'm going to finishg here: %x and return to: %x\n, index %i",  conn->finished_address , conn->restore_rip_address , i);
            break;
        } 
    }

    //If I'm on the instruction that's about to finish, then we need to prioritize finish to avoid a loop
    //Add instruction to the station and DON'T finish it
    //you have 2 choices here, either you process the string or use the opcodes and identify the instruction
    //the operands are most likely easier to detect as a string
    bool available_station_load = false;
    bool available_station_sum = false;
    if(strncmp(entry->disassembled_text, "mov", 3) == 0) {
        for(int i = 0; i < conn->total_reservation_stations; ++i){
            for(int j = 0; j < REG_IN_PROGRAM; ++j){
                if(!conn->stations[i]->in_use && conn->stations[i]->type == TYPE_LOAD){
                    if(conn->register_list[j].name != NULL){
                        if(strncmp(conn->register_list[j].name, reg1->name, 3)==0){
                            if (strncmp(&entry->disassembled_text[4], reg1->name, 3)==0 && !conn->register_list[j].in_use){
                                for(int k = 0; k < REG_IN_PROGRAM; ++k){
                                    if(conn->register_list[k].name != NULL){
                                        if(strncmp(conn->register_list[k].name, reg2->name, 2)==0){
                                            if (strncmp(&entry->disassembled_text[8], reg2->name, 2)==0 && !conn->register_list[k].in_use){
                                                conn->register_list[j].in_use = true;
                                                conn->register_list[k].in_use = true;
                                                conn->stations[i]->curr_inst = entry;
                                                conn->stations[i]->in_use = true;
                                                conn->stations[i]->current_timeout = LOAD_TIMEOUT; //we set the TIMEOUT to restart the instruction that just entered our station
                                                available_station_load = true;
                                                conn->skip_instruction = true; //is already in a station, do not execute yet
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if(strncmp(entry->disassembled_text, "add", 3) == 0) {
        for(int i = 0; i < conn->total_reservation_stations; ++i){
            for(int j = 0; j < REG_IN_PROGRAM; ++j){
                if(!conn->stations[i]->in_use && conn->stations[i]->type == TYPE_SUM){
                    if(conn->register_list[j].name != NULL){
                        if(strncmp(conn->register_list[j].name, reg1->name, 3)==0){
                            if (strncmp(&entry->disassembled_text[4], reg1->name, 3)==0 && !conn->register_list[j].in_use){
                                for(int k = 0; k < REG_IN_PROGRAM; ++k){
                                    if(conn->register_list[k].name != NULL){
                                        if(strncmp(conn->register_list[k].name, reg2->name, 2)==0){
                                            if (strncmp(&entry->disassembled_text[8], reg2->name, 2)==0 && !conn->register_list[k].in_use){
                                                conn->register_list[j].in_use = true;
                                                conn->register_list[k].in_use = true;
                                                conn->stations[i]->curr_inst = entry;
                                                conn->stations[i]->in_use = true;
                                                conn->stations[i]->current_timeout = SUM_TIMEOUT;
                                                available_station_sum = true;
                                                conn->skip_instruction = true; //is already in a station, do not execute yet
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //No station can take this instruction that was fetch, we need to stall until they free up
    if(!available_station_sum && !available_station_load && !conn->finish){
        SIM_LOG_INFO(1, obj, 0, "I need to stall");
        conn->stall = true;
        conn->stall_address = address; //we'll loop in the current RIP until some engine finished and removes the stall
        return;
    } 
    /*Here you can add all the other stations that you want, they can follow the usage of the same APIs*/
}

/*
 * This function runs EVERY CYCLE and decrements the counters on each of the units 
 * */
void station_timers(conf_object_t * obj){
    connection_t *conn = conn_of_obj(obj);
    conf_object_t * cpu = conn->provider;
    for(int i = 0; i < conn->total_reservation_stations; ++i){
        if(conn->stations[i]->in_use && conn->stations[i]->current_timeout > 0) {
            --conn->stations[i]->current_timeout;
            if(conn->stations[i]->type==0){
                SIM_LOG_INFO(1, obj, 0, "Station LOAD decremented timeout to %x\n", conn->stations[i]->current_timeout);
            } else if(conn->stations[i]->type==1){
                SIM_LOG_INFO(1, obj, 0, "Station SUM decremented timeout to %x\n", conn->stations[i]->current_timeout);
            }   
        }
    }
}

/*This function is to fullfull the callback expectations. Has no functionality*/
static tuple_int_string_t
disassemble(conf_object_t *obj, conf_object_t *cpu, generic_address_t addr,
            cpu_bytes_t bytes) {
    return (tuple_int_string_t) {0, strdup("None") };
}

void dealloc(conf_object_t *obj, conf_object_t *cpu, void *user_data){

}

void add_finished_instruction(conf_object_t * obj, logical_address_t address){
    connection_t *conn = conn_of_obj(obj);
    //If the added instruction is already there do nothing
    for(int i = 0; i < conn->fetch_instruction_size; ++i) {
        if(conn->fetch_instructions[i].address == address){
            conn->finished_instructions[conn->finished_instructions_size++] = &conn->fetch_instructions[i];
            break;
        }
    }
}

void print_all_ordered_instructions(conf_object_t * obj){
    connection_t *conn = conn_of_obj(obj);
    SIM_LOG_INFO(3, obj, 0,"--- Trace of all finished instructions:");
    for(int i = 0; i< conn->finished_instructions_size; ++i){
        SIM_LOG_INFO(3, obj, 0,"Physical Address: 0x%x Dissasembly: %s", (int) conn->finished_instructions[i]->address, conn->finished_instructions[i]->disassembled_text);
    }
    SIM_LOG_INFO(3, obj, 0,"--- end of finished instructions");
}

/*
 * Handles the CPU state machine with the variables of the conn struct. 
 * For this exercise this doesn't require to be changed but you can change it if you want, at your own risk!
 * This operation is executed in every cycle.
 * it considers the following variables from the conn obj
 * restore_rip and restore_rip_address tell the CPU to restore the RIP to an original point in time if the flag is true
 * finish and finish_address: if the finish flag is set after operating over the instruction, then the next cycle will execute the instruction immediately (probably because the engine timeout expired)
 * stall and stall_address: if stall is set, the processor will stall cycles on that instruction pointer until told otherwise (probably because no stations are available)
 * skip: if skip is set, then the fetch instruction is not finished immediately (probably because it's in a station)
 * */
cpu_emulation_t
tomasulo_algorithm(conf_object_t *obj, conf_object_t *cpu, void *user_data){
    connection_t *conn = conn_of_obj(obj);
    logical_address_t address = (logical_address_t)((int *)user_data);

    //Even if the RIP changes in between don't print current instruction pointer
    if(!conn->restore_rip && !conn->stall) {
        SIM_LOG_INFO(1, obj, 0, "Current instruction pointer is at: 0x%x", (int)address );
    }
    else { //In case you want to print it all the time use log level 4
        SIM_LOG_INFO(4, obj, 0, "Current instruction pointer is at: 0x%x", (int)address );
    }
    //Entering a finish cycle, instruction pointer is in the right place, just execute
    if(conn->finish){
        SIM_LOG_INFO(1, obj, 0, "Finished instruction in address: 0x%x", (int)address);
        conn->finish = false;
        add_finished_instruction(obj, address);
        print_all_ordered_instructions(obj);
        station_timers(obj);
        ++conn->cycles;
        SIM_LOG_INFO(1, obj, 0, "Current CPU Cycle: %i", conn->cycles);
        return CPU_Emulation_Default_Semantics;
    }
    //Already finished, time to restore the rip to the former pointer
    if(conn->restore_rip){
        SIM_LOG_INFO(2, obj, 0, "Will restore instruction pointer to : 0x%x\n", (int) conn->restore_rip_address);
        conn->pi_iface->set_program_counter(cpu, conn->restore_rip_address); 
        conn->restore_rip = false;
        return CPU_Emulation_Control_Flow; 
    }
    //If it's in progress, I'm restoring the pointer from an earlier jump or I was stalling but no need to do fetch again
    bool in_process = already_processed(obj, address);
    if(in_process){
        SIM_LOG_INFO(3, obj, 0, "Instruction after restore or stall doesn't need to do fetch.");
        return CPU_Emulation_Fall_Through;
    }
    station_timers(obj);
    //This is the most important function where YOU control the CPU model to comply with Tomasulo's algorithm
    ++conn->cycles;
    SIM_LOG_INFO(1, obj, 0, "Current CPU Cycle: %i", conn->cycles);
    instruction_entry * entry = get_instruction_details(obj, address);
    if(strncmp(entry->disassembled_text,"add byte",8)!=0){
        if(REG_DEPENDENCY){
            identify_instruction_and_reg(obj, cpu, address);
        }else {
            identify_instruction_and_operand(obj, cpu, address);
        }
    }else{
        SIM_LOG_INFO(1, obj, 0, "Complete fetching the instructions of x86 program");
    }

    //Stall the CPU
    if(conn->stall){
        conn->pi_iface->set_program_counter(cpu, conn->stall_address);
        return CPU_Emulation_Control_Flow; 
    }
    //If finish was set after the call, move the instruction pointer for the next call to finish the instruction
    if(conn->finish) {
        conn->pi_iface->set_program_counter(cpu, conn->finished_address); //Move to address of the finished instruction
        conn->skip_instruction = false; //if we're finishing
        return CPU_Emulation_Control_Flow;
    }
    //We skipped the execution of this instruction, ie is inside a station and cannot be finished yet
    if(conn->skip_instruction){
        conn->skip_instruction = false;
        return CPU_Emulation_Fall_Through;    
    } 

    //We should NEVER enter this case, but to not break execution will add it here 
    if(conn->restore_rip){
        SIM_LOG_INFO(2, obj, 0,"NOT EXPECTED: Will restore instruction pointer to: 0x%x", (int) conn->restore_rip_address);
        conn->restore_rip = false;
        conn->pi_iface->set_program_counter(cpu, conn->restore_rip_address); 
        return CPU_Emulation_Control_Flow; 
    }
    //This should only happen if the instruction is immediately finished
    return CPU_Emulation_Default_Semantics;
}

/*
 * This function is called automatically, adds the instruction to the fetch instruction list
 * */
void add_instruction_data(conf_object_t * obj, logical_address_t pa, char * disasm){
    connection_t *conn = conn_of_obj(obj);
    //If the added instruction is already there do nothing
    for(int i = 0; i < conn->fetch_instruction_size; ++i) {
        if(conn->fetch_instructions[i].address == pa)
            return;
    }
    conn->fetch_instructions[conn->fetch_instruction_size].address = pa;
    conn->fetch_instructions[conn->fetch_instruction_size++].disassembled_text = strdup(disasm);
}

/*
 * Helper function to print all fetch instructions
 * */
void print_all_instruction_data(conf_object_t * obj){
    connection_t *conn = conn_of_obj(obj);
    SIM_LOG_INFO(3, obj, 0,"Trace of all fetch instruction:");
    for(int i = 0; i< conn->fetch_instruction_size; ++i){
        SIM_LOG_INFO(3, obj, 0,"Physical Address: 0x%x Dissasembly: %s", (int) conn->fetch_instructions[i].address, conn->fetch_instructions[i].disassembled_text);
    }
    SIM_LOG_INFO(3, obj, 0,"--- end of fetch instructions");
}



/*
 * Here is where the CPU decoder encounters the instruction and adds it to the list of fetch instructions
 *
 * You don't need to change it's code, since all the algorithm is handled elsewhere
 * */
static int
gather_instruction_data(conf_object_t *obj, conf_object_t *cpu,
        decoder_handle_t *handle,
        instruction_handle_t *i_handle,
        void * data){
    connection_t *conn = conn_of_obj(obj);
    logical_address_t la = conn->iq_iface->logical_address(cpu, i_handle); //get the logical address of the instruction
    cpu_bytes_t bytes = conn->iq_iface->get_instruction_bytes(cpu, i_handle); //get the bytes
    attr_value_t op_code = SIM_make_attr_data(bytes.size, bytes.data); //Makes a data object
    tuple_int_string_t disasm = conn->pi_iface->disassemble(cpu, (physical_address_t) la, op_code, 0);
    add_instruction_data(obj, la, disasm.string);
    print_all_instruction_data(obj);
    conn->ir_iface->register_emulation_cb(cpu, tomasulo_algorithm, handle, (void *) la, dealloc); 
    return disasm.integer;
}

/*
 * This call adds the station to the list to be later decremented with time. 
 * ALl stations start as NOT in use.
 * */
void init_add_station(conf_object_t * obj, unit_functional_station * station){
    connection_t *conn = conn_of_obj(obj);
    station->in_use = false;
    station->current_timeout = 0;
    conn->stations[conn->total_reservation_stations++] = station;
}

/*
 *Here add all the code required to create a new station
 * */
void init_all_stations(conf_object_t * obj){
    unit_functional_station * new_station =  MM_ZALLOC(1, unit_functional_station);
    new_station->type = TYPE_SUM;
    new_station->current_timeout = SUM_TIMEOUT;
    init_add_station(obj, new_station);
    new_station =  MM_ZALLOC(1, unit_functional_station);
    new_station->type = TYPE_LOAD;
    new_station->current_timeout = LOAD_TIMEOUT;
    init_add_station(obj, new_station);
    /*Here you can add stations of different types and ALWAYS call the init_add_station function to record it in the stations array so that 
     * it's decremented every cycle.
     * EXAMPLE:
     * new_station =  MM_ZALLOC(1, unit_functional_station);
     * new_station->the_var_I_created = info_I_need;
     * init_add_station(obj, new_station);
     * */
}



/* Help function which creates a new connection and returns it.
 * This function is not important to this excercise it creates all the necessary
 * variables within the connection_t (tomasulo engine)
 * */
static connection_t *
new_connection(tomasulo_t *tool, conf_object_t *provider, attr_value_t args)
{
        strbuf_t sb = SB_INIT;
        
        sb_addfmt(&sb, "%s_%d",
                  SIM_object_name(&tool->obj),
                  tool->next_connection_number);
        conf_object_t *conn_obj = SIM_create_object(
                connection_class, sb_str(&sb), args);
        sb_free(&sb);
        
        if (!conn_obj)
                return NULL;

        tool->next_connection_number++;
        connection_t *conn = conn_of_obj(conn_obj);
        conn->provider = provider;

        /* Cache interfaces */
        conn->cis_iface = SIM_C_GET_INTERFACE(
                provider, cpu_instrumentation_subscribe);
        conn->cci_iface = SIM_C_GET_INTERFACE(
                provider, cpu_cached_instruction);
        conn->pi_iface = SIM_C_GET_INTERFACE(
                provider, processor_info_v2);
        conn->iq_iface = SIM_C_GET_INTERFACE(
                        provider, cpu_instruction_query);
        conn->ir_iface = SIM_C_GET_INTERFACE(
                provider, cpu_instruction_decoder);
                        
	    conn->cis_iface->register_instruction_decoder_cb(provider, conn_obj,
                                                   gather_instruction_data, disassemble, NULL);
        conn->restore_rip = false;
        conn->restore_rip_address = -1;
        conn->finish = false;
        conn->finished_address = -1;
        conn->stall = false;
        conn->stall_address = -1;
        conn->skip_instruction = false;
        conn->fetch_instruction_size = 0;
        conn->finished_instructions_size = 0;
        conn->cycles = 0;
        init_all_stations(conn_obj);
        return conn;
}

/* ALL the code below should be ignored and is used for the Simics framework to create the objects in memory*/
static attr_value_t
get_provider(void *param, conf_object_t *obj, attr_value_t *idx)
{
        connection_t *conn = conn_of_obj(obj);
        return SIM_make_attr_object(conn->provider);
}

/************************** Tool Object ********************************/

// instruction_count_tool::alloc
static conf_object_t *
it_alloc(void *arg)
{
        tomasulo_t *tool = MM_ZALLOC(1, tomasulo_t);
        VINIT(tool->connections);
        return &tool->obj;
}

static int
it_delete_connection(conf_object_t *obj)
{
        tomasulo_t *tool = tool_of_obj(obj);
        ASSERT_FMT(VEMPTY(tool->connections),
                   "%s deleted with active connections", SIM_object_name(obj));
        MM_FREE(tool);
        return 0;
}

// instrumentation_tool::connect()
static conf_object_t *
it_connect(conf_object_t *obj, conf_object_t *provider, attr_value_t args)
{
        tomasulo_t *tool = tool_of_obj(obj);
        connection_t *conn = new_connection(tool, provider, args);
        if (!conn)
                return NULL;
        
        VADD(tool->connections, conn);
        
        return &conn->obj;
}

// instrumentation_tool::disconnect()
static void
it_disconnect(conf_object_t *obj, conf_object_t *conn_obj)
{
        tomasulo_t *tool = tool_of_obj(obj);
        VREMOVE_FIRST_MATCH(tool->connections, conn_of_obj(conn_obj));
        SIM_delete_object(conn_obj);
}

static attr_value_t
get_connections(void *param, conf_object_t *obj, attr_value_t *idx)
{
        tomasulo_t *tool = tool_of_obj(obj);
        
        attr_value_t ret = SIM_alloc_attr_list(VLEN(tool->connections));
        VFORI(tool->connections, i) {
                SIM_attr_list_set_item(
                        &ret, i,
                        SIM_make_attr_object(&VGET(tool->connections, i)->obj));
        }
        return ret;
}

static void
init_tool_class(void)
{
        const class_data_t funcs = {
                .alloc_object = it_alloc,
                .delete_instance = it_delete_connection,
                .class_desc = "Implements the tomasulo algorithm",
                .description = "This tool takes all the instructions from the provider"
                " and applies the tomasulo algorithm on them."
                " processor.",
                .kind = Sim_Class_Kind_Session
        };

        conf_class_t *cl = SIM_register_class("tomasulo", &funcs);
        
        static const instrumentation_tool_interface_t it_iface = {
                .connect = it_connect,
                .disconnect = it_disconnect
        };
        SIM_REGISTER_INTERFACE(cl, instrumentation_tool, &it_iface);        

        SIM_register_typed_attribute(
               cl, "connections",
               get_connections, NULL,
               NULL, NULL,
               Sim_Attr_Pseudo,
               "[o*]", NULL,
               "The connection objects currently used.");
}

static void
init_connection_class(void)
{
        const class_data_t funcs = {
                .alloc_object = ic_alloc,
                .pre_delete_instance = ic_pre_delete_connection,
                .class_desc = "instrumentation connection",
                .description = "Instrumentation connection.",
                .delete_instance = ic_delete_connection,
                .kind = Sim_Class_Kind_Session
        };
        connection_class = SIM_register_class("tomasulo_connection_t",
                                              &funcs);
        
        static const instrumentation_connection_interface_t ic_iface = {
                .enable = ic_enable,
                .disable = ic_disable,
        };
        SIM_REGISTER_INTERFACE(connection_class,
                               instrumentation_connection, &ic_iface);

        SIM_register_typed_attribute(
                connection_class, "provider",
                get_provider, NULL,
                NULL, NULL,
                Sim_Attr_Pseudo,
                "o", NULL,
                "The provider of the connection.");
}

void
init_local(void)
{
        init_tool_class();
        init_connection_class();
}
