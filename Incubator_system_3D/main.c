/*
 * avrapp1.c
 *
 * Created: 8/31/2023 2:41:38 PM
 * Author : 20115
 */ 

#include "main.h"


adc_config temp_sensor;
gpio_pin_config dc_fan1;
gpio_pin_config dc_fan2;
gpio_pin_config ac_lamp;
pwm_config dc_fan_pwm;
timer0_config timer;



uint8 receivedData;
uint8 index;
//frame: @  M  L  v  v  v  H  v  v  v   F  v  v   v   B   ; 
//frame: 0  1  2  3  4  5  6  7  8  9  10 11 12  13  14  15
uint8 recieved_frame[16];
uint16 lower_temp;
uint16 upper_temp;
uint16 fan_speed;
uint16 temp_val;
uint8 isr_count;
uint8 temp_status;
uint8 temp_str[3]="000";



int main(void)
{
	
	intiallize_temp_sensor();
	intiallize_DC_fan();
	intiallize_ac_lamp();
	LCD_init();
	// Set baud rate to 9600 bps
	USART_Init(51);

	// Enable global interrupts
	sei();

    while (1) 
    {		
		temp_status_display();
		LCD_goToRowColumn(1,0);
		mcal_adc_read_poilling_value(&temp_sensor,&temp_val);
		temp_to_string(temp_str,temp_val);
		LCD_displayStringRowColumn(1,0,"temperature=");
		LCD_displayStringRowColumn(1,12,temp_str);
		auto_mode_handle_and_lcd_status_update();
		
	}
}

void intiallize_temp_sensor()
{
	adc_config temp_sensor={
		.refrence=adc_AVCC_pin,
		.adjust=adc_right_adjust,
		.channel=adc_CH0,
		.prescalar=adc_divide_by_64//125 kHz
	};
	
	mcal_adc_intiallize(&temp_sensor);
	//mcal_adc_read_interrupt_value();
}

void intiallize_DC_fan()
{
	gpio_pin_config dc_fan1={
		.port=portB,
		.pin=pin4,
		.direction=output,
		.logic=low,
	};
	mcal_gpio_pin_direction_intiallize(&dc_fan1);
	mcal_gpio_pin_output_write_logic(&dc_fan1,dc_fan1.logic);
	
	gpio_pin_config dc_fan2={
		.port=portB,
		.pin=pin5,
		.direction=output,
		.logic=high,
	};
	mcal_gpio_pin_direction_intiallize(&dc_fan2);
	mcal_gpio_pin_output_write_logic(&dc_fan2,dc_fan2.logic);
	
	pwm_config dc_fan_pwm={
		.mode=fast_pwm_mode,
		.clock_prescale=prescalar_divide_by_64,
	};
	mcal_PWM0_intiallize(&dc_fan_pwm);
	OCR0=0;
}

void intiallize_ac_lamp()
{
	ac_lamp.direction=output;
	ac_lamp.port=portB;
	ac_lamp.pin=pin6;
	ac_lamp.logic=high;
	mcal_gpio_pin_direction_intiallize(&ac_lamp);
}


// Interrupt service routine for USART Receive Complete
ISR(USART_RXC_vect) {
	cli();
	receivedData = UDR;
	recieved_frame[index]=receivedData;
	index++;
	if (index==16)
	{
		isr_count++;
		if (recieved_frame[0]=='@')
		{
			switch(recieved_frame[1])
			{
				case 'A':
				{
					lower_temp=getvalue(3);
					upper_temp=getvalue(7);
					//high
					if(temp_val > upper_temp)
					{
						PWM0_CLEAR_ON_UP_COUNTING();
						mcal_PWM0_increase_to_value(200);
						mcal_gpio_pin_output_write_logic(&ac_lamp,low);
						USART_TransmitString("H%");
						temp_status=2;
					}
					//low
					if(temp_val < lower_temp)
					{
						if(OCR0==200)
						{
							mcal_PWM0_decrease_from_value(200);
						}
						else{OCR0=0;}
						mcal_gpio_pin_output_write_logic(&ac_lamp,high);
						USART_TransmitString("L%");
						temp_status=1;
					}
				
					if ((temp_val>=lower_temp)&&(temp_val<=upper_temp))
					{
						if (OCR0>0)
						{
							mcal_PWM0_decrease_from_value(OCR0);
						}
						mcal_gpio_pin_output_write_logic(&ac_lamp,low);
						USART_TransmitString("N%");
						temp_status=0;
					}
					break;	
				}
	
				
				
				//Manual
				case 'M':
				{
					lower_temp=getvalue(3);
					upper_temp=getvalue(7);
					if(temp_val > upper_temp)
					{
						USART_TransmitString("H%");
						temp_status=2;
					}
					else if(temp_val < lower_temp)
					{
						USART_TransmitString("L%");
						temp_status=1;
					}
					else if ((temp_val>=lower_temp)&&(temp_val<=upper_temp))
					{
						USART_TransmitString("N%");
						temp_status=0;
					}
					switch(recieved_frame[10])
					{
						case'F':
						fan_speed=getvalue(11);
						mcal_PWM0_start_to_end(OCR0,fan_speed);
						break;
						case'X':
						mcal_PWM0_decrease_from_value(OCR0);
						break;
					}
					if(recieved_frame[14]=='B'){
						//bulb is ON
						mcal_gpio_pin_output_write_logic(&ac_lamp,high);
					}
					else
					{
						mcal_gpio_pin_output_write_logic(&ac_lamp,low);
					}
					break;
				}
			

				
				//Status
				case'S':
				{
					if(temp_val > upper_temp)
					{
						USART_TransmitString("H%");
						temp_status=2;
					}
					else if(temp_val < lower_temp)
					{
						USART_TransmitString("L%");
						temp_status=1;
					}
					else if((temp_val>=lower_temp)&&(temp_val<=upper_temp))
					{
						USART_TransmitString("N%");
						temp_status=0;
					}
					break;
				}
				
			}
		}
		index=0;
	}
	sei();
}

void auto_mode_handle_and_lcd_status_update()
{
	if(recieved_frame[1]=='A')
	{lower_temp=getvalue(3);
	upper_temp=getvalue(7);
	//high
	if(temp_val > upper_temp)
	{
		PWM0_CLEAR_ON_UP_COUNTING();
		mcal_PWM0_increase_to_value(200);
		mcal_gpio_pin_output_write_logic(&ac_lamp,low);
		USART_TransmitString("H%");
		temp_status=2;
	}
	//low
	if(temp_val < lower_temp)
	{
		if(OCR0==200)
		{
			mcal_PWM0_decrease_from_value(200);
		}
		else{OCR0=0;}
		mcal_gpio_pin_output_write_logic(&ac_lamp,high);
		USART_TransmitString("L%");
		temp_status=1;
	}
	
	if ((temp_val>=lower_temp)&&(temp_val<=upper_temp))
	{
		if (OCR0>0)
		{
			mcal_PWM0_decrease_from_value(OCR0);
		}
		mcal_gpio_pin_output_write_logic(&ac_lamp,low);
		USART_TransmitString("N%");
		temp_status=0;
	}
	}
	else if(recieved_frame[1]=='M')
	{
		if(temp_val > upper_temp)
		{
			temp_status=2;
			USART_TransmitString("H%");
		}
		if(temp_val < lower_temp)
		{
			temp_status=1;
			USART_TransmitString("L%");
		}
		if ((temp_val>=lower_temp)&&(temp_val<=upper_temp))
		{
			temp_status=0;
			USART_TransmitString("N%");
		}
	}
}



uint16 getvalue(uint8 starting_index){
	uint8 end_index=starting_index+3;
	uint16 value=0;
	for(uint8 i=starting_index;i<end_index;i++)
	{
		value+=(recieved_frame[i]-48);
		if(i<end_index-1)
		{
			value*=10;
		}
	}
	return value;
}

void temp_status_display()
{
	if(isr_count>0)
	{
		if(temp_val > upper_temp)
		{
			temp_status=2;
		}
		if(temp_val < lower_temp)
		{
			temp_status=1;
		}
		if ((temp_val>=lower_temp)&&(temp_val<=upper_temp))
		{
			temp_status=0;
		}
	}
	switch(temp_status)
	{
		case 0:
		LCD_displayStringRowColumn(0,0,"normal");
		break;
		case 1:
		LCD_displayStringRowColumn(0,0,"cold  ");
		break;
		case 2:
		LCD_displayStringRowColumn(0,0,"hot   ");
		break;
	}
}

void temp_to_string(uint8 str[], uint16 temp_int)
{
	 uint8 i, rem, len = 0, n;
	 
	 n = temp_int;
	/* while (n != 0)
	 {
		 len++;
		 n /= 10;
	 }*/
	 for (i = 0; i < 3; i++)
	 {
		 rem = temp_int % 10;
		 temp_int = temp_int / 10;
		 str[3 - (i + 1)] = rem + '0';
	 }
	 str[3] = '\0';
}