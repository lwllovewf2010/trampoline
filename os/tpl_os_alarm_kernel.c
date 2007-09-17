/**
 * @file tpl_os_alarm_kernel.c
 *
 * @section desc File description
 *
 * Trampoline Alarm Kernel implementation file
 *
 * @section copyright Copyright
 *
 * Trampoline OS
 *
 * Trampoline is copyright (c) IRCCyN 2005-2007
 * Trampoline is protected by the French intellectual property law.
 *
 * This software is distributed under the Lesser GNU Public Licence
 *
 * @section infos File informations
 *
 * $Date$
 * $Rev$
 * $Author$
 * $URL$
 */

#include "tpl_os_definitions.h"
#include "tpl_os_kernel.h"
#include "tpl_os_alarm_kernel.h"

#define OS_START_SEC_CODE
#include "tpl_memmap.h"

/*
 * tpl_insert_time_obj
 * insert a time object in the time object queue of the counter
 * it belongs to.
 *
 * The time object list of a counter is a double-linked list
 * and a time object is inserted starting from the
 * head of the list
 */
void tpl_insert_time_obj(tpl_time_obj *time_obj)
{
    /*  get the counter                                                     */
    tpl_counter     *counter = time_obj->stat_part->counter;
    /*  initialize the current time object to the head                      */
    tpl_time_obj    *current_to = counter->first_to;
    /*  initialize the time object that precede the current one to NULL     */
    tpl_time_obj    *prev_to = NULL;
    
    if (current_to == NULL)
    {
        /*  The time object queue is empty
            So the time object is alone in the queue                        */
        counter->first_to = time_obj;
        counter->next_to = time_obj;
        time_obj->next_to = time_obj->prev_to = NULL;
    }
    else
    {
        /*  The time object queue is not empty
            look for the place to insert the alarm                          */
        while ((current_to != NULL) &&
               (current_to->date <= time_obj->date))
        {
            prev_to = current_to;
            current_to = current_to->next_to;
        }
    
        time_obj->next_to = current_to;
        time_obj->prev_to = prev_to;
    
        /*  insert the alarm    */
        if (current_to != NULL)
        {
            current_to->prev_to = time_obj;
        }
        if (prev_to != NULL)
        {
            /*  add at the end of the queue or insert                       */
            prev_to->next_to = time_obj;
        }
        else
        {
            /*  the condition current_to->date <= time_object->date was
                false a the beginning of the while. So the time object
                have to be added at the head of the time object queue       */
            counter->first_to = time_obj;
        }
        
        /*  Update the next_to to point to the newly
            inserted time_object if the date of the newly inserted time
            object is within the current date and the next_alarm_to_raise
            date, taking account the modulo                                 */
        if (counter->next_to->date < counter->current_date)
        {
            if ((time_obj->date > counter->current_date) || 
                (time_obj->date < counter->next_to->date))
            {      
                counter->next_to = time_obj;
            }
        }
        else
        {
            if ((time_obj->date > counter->current_date) &&
                (time_obj->date < counter->next_to->date))
            {
                counter->next_to = time_obj;
            }
        }
    }
    
    /*  Anyway, the alarm is put in the active state    */
    time_obj->state |= ALARM_ACTIVE;
}

/*
 * tpl_remove_alarm
 * remove an alarm from the alarm queue of the counter
 * it belongs to.
 */
void tpl_remove_time_obj(tpl_time_obj *time_obj)
{
    
    tpl_counter *counter = time_obj->stat_part->counter;
    
    /*  adjust the head of the queue if the 
        removed alarm is at the head            */
    if (time_obj == counter->first_to)
    {
        counter->first_to = time_obj->next_to;
    }
    /*  adjust the next alarm to raise if it is
        the removed alarm                       */
    if (time_obj == counter->next_to)
    {
        counter->next_to = time_obj->next_to;
    }
    /*  build the link between the previous and next alarm in the queue */
    if (time_obj->next_to != NULL)
    {
        time_obj->next_to->prev_to = time_obj->prev_to;
    }
    if (time_obj->prev_to != NULL)
    {
        time_obj->prev_to->next_to = time_obj->next_to;
    }
    /*  if the next_alarm_to_raise was pointing to the
        alarm and the alarm was at the end of the queue
        next_alarm_to_raise is NULL and must be reset to
        the first alarm of the queue                        */
    if (counter->next_to == NULL)
    {
        counter->next_to = counter->first_to;
    }
    
    /*  The alarm is put in the sleep state */
    time_obj->state = ALARM_SLEEP;
}

/**
 * @internal
 *
 * tpl_raise_alarm is called by tpl_counter_tick
 * when an alarm time object is raised.
 *
 * @param time_obj  The alarm to raise.
 */
tpl_status tpl_raise_alarm(tpl_time_obj *time_obj)
{
    tpl_status  result = E_OK;
    
    /*  Get the alarm descriptor                            */
    tpl_alarm_static    *stat_alarm = (tpl_alarm_static *)time_obj->stat_part;
    /*  Get the action to perform from the alarm descriptor */
    const tpl_action    *action_desc = stat_alarm->action;
    
    /*  Call the action                                     */
    result = (action_desc->action)(action_desc) ;
    
    return result;
}


/*
 * tpl_counter_tick is called by the IT associated with a counter
 * The param is a pointer to the counter
 * It increment the counter tick and the counter value if needed
 * If the counter value is incremented, it checks the next alarm
 * date and raises alarms at that date.
 *
 * suggested modification by Seb - 2005-02-01
 *
 * Update: 2006-12-10: Does not perform the rescheduling. 
 * tpl_schedule must be called explicitly
 */
tpl_status tpl_counter_tick(tpl_counter *counter)
{
    tpl_time_obj*   t_obj;
    tpl_expire_func expire;
    tpl_tick        date;
    tpl_status      need_resched = NO_SPECIAL_CODE;
    
    /*  inc the current tick value of the counter   */
    counter->current_tick++;
    /*  if tickperbase is reached, the counter is inc   */
    if (counter->current_tick == counter->ticks_per_base)
    {
        counter->current_tick = 0;
        counter->current_date++;
        date = counter->current_date;
        /*  check if the counter has reached the
            next alarm activation date  */
        t_obj = counter->next_to;

        while ((t_obj != NULL) && (t_obj->date == date))
        {
            /*  note : time_obj is always the next_to
                since removing the time object from the queue will
                advance next_to along the queue                     */
            
            /*  get the time object from the queue                  */
            tpl_remove_time_obj(t_obj);
            /*  raise it    */
            expire = t_obj->stat_part->expire;
            need_resched |=
                (TRAMPOLINE_STATUS_MASK & expire(t_obj));

            /*  rearm the alarm if needed   */
            if (t_obj->cycle != 0)
            {
                /*  if the cycle is not 0,
                    the new date is computed
                    by adding the cycle to the current date         */
                t_obj->date += t_obj->cycle;
                /*  and the alarm is put back in the alarm queue
                    of the counter it belongs to                    */
                tpl_insert_time_obj(t_obj);
            }
    
            /*  get the next alarm to raise     */
            t_obj = counter->next_to;
        }
    }
    return need_resched;
}

#define OS_STOP_SEC_CODE
#include "tpl_memmap.h"

/* End of file tpl_alarm_kernel.c */