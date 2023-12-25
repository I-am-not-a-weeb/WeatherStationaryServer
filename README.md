## HTTP
#### GET /data/:DATE 

    {
	    "filter":
	    {
			"time":
			{
				"hour":int,
				"mins":int,
				"secs":int
			}.
			"temp":
			{
				"lt":int,	//less than
				"gt":int	//greater than
			},
			"humi":
			{
				"lt":int,	//less than
				"gt":int	//greater than
			},
			"ppm":
			{
				"lt":int,	//less than
				"gt":int	//greater than
			},
			"lux":
			{
				"lt":int,	//less than
				"gt":int	//greater than
			},
			"rpm":
			{
				"lt":int,	//less than
				"gt":int	//greater than
			}
		}

    }
