#include"trace-link.hh"
TraceLink::TraceLink(int fd,bool t_output_enable,std::string t_link_name,std::string trace_file) :
   Link(fd,t_output_enable,t_link_name) ,
   last_token_update(begin_time),
   token_count(0),
   BURST_SIZE(1600), /* 1 packet */
   pkt_schedule(PktSchedule (trace_file)) {
   token_count=pkt_schedule.current_byte_credit;
   std::cout<<"Starting out with byte credit "<<token_count<<" at time "<<begin_time<<"\n"; 
}

int TraceLink::recv(uint8_t* ether_frame,uint16_t size) {
     Payload p(ether_frame,size);
     return enqueue(p); 
}

void TraceLink::tick() {
   uint64_t ts_now=Link::timestamp(); 
   Link::print_stats(ts_now);
   update_token_count(ts_now,0);
   /* Can I send pkts right away ? */ 
   if(!pkt_queue.empty()) { 
     Payload head=pkt_queue.front();
     while(token_count>=head.size && head.size > 0) {
        send_pkt(head);
        dequeue();
        ts_now=Link::timestamp();  
        update_token_count(ts_now,head.size);
        if(pkt_queue.empty()) head.size=-1;
        else  head=pkt_queue.front();
     }
     /* if there are packets wait till tokens accumulate in the future */ 
     if(!pkt_queue.empty())  {
      /* calc next_transmission when you acc. sufficient credit */
      next_transmission=calc_next_time(head.size);
     }
     else next_transmission = (uint64_t)-1;
   }
   else next_transmission = (uint64_t)-1 ;
}

void TraceLink::update_token_count(uint64_t current_ts,long double drain) {
     /* check if you see a more recent line */
     uint32_t current_byte_credit=0 ; /* If there is a new point in the file , then update this */ 
     if((current_ts-begin_time)>=pkt_schedule.next_timestamp) {  
        assert((!pkt_schedule.pkt_list.empty())); 
        current_byte_credit=std::get<1>(pkt_schedule.pkt_list.front()); 
//        std::cout<<link_name<<" Added credit of "<<current_byte_credit<<" bytes at time "<<current_ts<<" \n";
        /* House keeping on pkt schedule */
        pkt_schedule.pkt_list.pop_front(); 
        if(!pkt_schedule.pkt_list.empty()) {
          pkt_schedule.next_timestamp=std::get<0>(pkt_schedule.pkt_list.front());
        }
        else {
          std::cout<<link_name<<" bad luck, no more credit left, kill simulation \n";
          exit(5);
        }
     }
     /* new token count in bytes */
     long double new_token_count=token_count+current_byte_credit-drain; 
     last_token_update=current_ts;
     token_count=(new_token_count > BURST_SIZE) ? BURST_SIZE : new_token_count;
}

TraceLink::PktSchedule::PktSchedule(std::string t_file_name) :
     file_name(t_file_name) ,
     next_timestamp(-1),
     current_byte_credit(0),
     pkt_list(std::list<std::tuple<uint64_t,uint32_t>>()) {
     /* populate pkt_list using the file */
     std::ifstream pkt_stream (file_name.c_str());
     uint64_t time;
     uint32_t bytes;
     if(!pkt_stream.good()) {
         std::cout<<"Trace file "<<file_name<<" does not exist ... exiting \n";
         exit(-5);
     } 
     while (true) {
       pkt_stream>>time>>bytes;
//       std::cout<<"Time in nseconds is "<<time<<"\n"; 
       if( pkt_stream.eof() ) break;
       pkt_list.push_back(std::tuple<uint64_t,uint32_t>(time,bytes)); /* time in seconds */ 
     }
     assert(pkt_list.size()>=2);
     current_byte_credit=std::get<1>(pkt_list.front());
     pkt_list.pop_front();
     next_timestamp=std::get<0>(pkt_list.front());
}

uint64_t TraceLink::calc_next_time(uint32_t head_size) {
     long double required_tokens=head_size-token_count;
      /* Run through the pkt_list to check how long before you accumulate sufficient tokens */ 
     long double acc=0; 
     for (std::list<std::tuple<uint64_t,uint32_t>>::iterator it = pkt_schedule.pkt_list.begin(); it != pkt_schedule.pkt_list.end(); it++) {
        std::tuple<uint64_t,uint32_t> current=*it;
        acc=acc+std::get<1>(current);
        if(acc>=required_tokens) {
          return std::get<0>(current)+begin_time;
          /* because the trace timestamps are relative
             to the beginning of the link going on */
        }
     }
     std::cout<<"No more credit to transmit this packet \n";
     return -1; /* If there is no credit left */ 
}
